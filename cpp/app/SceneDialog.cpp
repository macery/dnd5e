#include "SceneDialog.hpp"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "WindowSizing.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

QString signedBonus(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}

}  // namespace

SceneDialog::SceneDialog(dnd::Repository& repository, dnd::AdventureRun& run, const dnd::AdventureTemplate& adventure,
                         const dnd::AdventureScene& scene, const QString& title, QWidget* parent)
    : QDialog(parent), repository_(repository), run_(run), adventure_(adventure), scene_(scene) {
    setWindowTitle(title);
    party_ = dnd::party_rollers(repository_, run_.party);
    if (scene_.kind == dnd::SceneKind::Social) npc_ = dnd::find_run_npc(repository_, run_, scene_.npc);

    auto* narration = new QLabel(QString("<b>%1.</b> %2").arg(q(scene_.title).toHtmlEscaped(),
                                                            q(scene_.narration).toHtmlEscaped()),
                                 this);
    narration->setWordWrap(true);
    npcLabel_ = new QLabel(this);
    npcLabel_->setWordWrap(true);
    npcLabel_->setVisible(npc_.has_value());
    readButton_ = new QPushButton(this);
    readButton_->setVisible(npc_.has_value());
    readButton_->setToolTip("Insight (DMG ch. 8): learn what they want and fear. Bringing those up makes every "
                            "request easier (DC -5). One try per conversation, by whoever is chosen below.");

    optionList_ = new QListWidget(this);
    approachCombo_ = new QComboBox(this);
    approachCombo_->setToolTip("How you go about it. The DC comes from what you ask and how they feel about you "
                               "(DMG ch. 8).");
    rollerCombo_ = new QComboBox(this);
    helperCombo_ = new QComboBox(this);
    helperCombo_->setToolTip("Help (PHB): another character who could do it too gives advantage on the check.");
    inspirationBox_ = new QCheckBox("Use inspiration (advantage)", this);
    attemptButton_ = new QPushButton("Roll", this);
    resultLabel_ = new QLabel(this);
    resultLabel_->setWordWrap(true);

    auto* form = new QFormLayout();
    form->addRow("Approach", approachCombo_);
    form->addRow("Who", rollerCombo_);
    form->addRow("Helped by", helperCombo_);
    form->addRow("", inspirationBox_);
    form->setRowVisible(approachCombo_, npc_.has_value());

    auto* buttons = new QDialogButtonBox(this);
    continueButton_ = buttons->addButton("Continue", QDialogButtonBox::AcceptRole);
    continueButton_->setEnabled(false);
    auto* later = buttons->addButton("Stop for Now", QDialogButtonBox::RejectRole);
    later->setToolTip("Leave the adventure here; resume it from Solo Adventure.");
    connect(continueButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(later, &QPushButton::clicked, this, &QDialog::reject);

    auto* content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->addWidget(narration);
    contentLayout->addWidget(npcLabel_);
    contentLayout->addWidget(readButton_);
    contentLayout->addWidget(optionList_);
    contentLayout->addLayout(form);
    contentLayout->addWidget(attemptButton_);
    contentLayout->addWidget(resultLabel_);
    contentLayout->addStretch(1);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scrollable(content, this), 1);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(560, 520), "Scene");

    connect(optionList_, &QListWidget::currentRowChanged, this, [this](int) { refreshOption(); });
    connect(approachCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshRoller(); });
    connect(rollerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshRoller(); });
    connect(attemptButton_, &QPushButton::clicked, this, &SceneDialog::attempt);
    connect(readButton_, &QPushButton::clicked, this, &SceneDialog::readNpc);

    if (scene_.kind == dnd::SceneKind::Trap) {
        buildTrapOptions();
    } else {
        for (const dnd::SceneOption* option : dnd::available_options(scene_, run_)) options_.push_back(*option);
    }
    // Someone dead, or too hostile for anything the scene asks: the party can still move on.
    if (npc_ && std::none_of(options_.begin(), options_.end(), [this](const dnd::SceneOption& option) {
            return !option.social || (!npc_->dead && dnd::conversation_dc(*npc_, option.request).has_value());
        })) {
        dnd::SceneOption onward;
        onward.label = "Move on";
        onward.success.text = npc_->dead ? "There's no one left here to talk to." : "There's nothing more to say.";
        options_.push_back(onward);
    }
    refreshNpc();
    fillOptions();  // selects the first option that can be tried
    refreshOption();
}

void SceneDialog::refreshNpc() {
    if (!npc_) return;
    const dnd::Npc& npc = *npc_;
    QString text = QString("<b>%1</b>, %2 — <i>%3</i>.<br>%4")
                       .arg(q(npc.name).toHtmlEscaped(), q(npc.role).toHtmlEscaped(),
                            npc.dead ? "dead" : dnd::attitude_name(npc.attitude), q(npc.description).toHtmlEscaped());
    if (npc.known) {
        text += QString("<br><b>Wants:</b> %1. <b>Fears:</b> %2. (DCs are %3 lower.)")
                    .arg(q(npc.wants).toHtmlEscaped(), q(npc.fears).toHtmlEscaped())
                    .arg(dnd::kKnownMotivesBonus);
    } else {
        text += "<br>You don't know yet what they want or fear.";
    }
    npcLabel_->setText(text);
    readButton_->setText(QString("Read them (Insight, DC %1)").arg(scene_.insight_dc));
    readButton_->setVisible(!npc.known && !npc.dead);
    readButton_->setEnabled(!readTried_ && !done_);
}

void SceneDialog::fillOptions() {
    int row = optionList_->currentRow();
    optionList_->blockSignals(true);
    optionList_->clear();
    for (const auto& option : options_) {
        QString label = q(option.label);
        bool possible = true;
        if (option.social && npc_) {
            auto dc = dnd::conversation_dc(*npc_, option.request);
            possible = dc.has_value() && !npc_->dead;
            QString ask = dnd::social_request_text(option.request);
            if (npc_->dead) {
                label += QString(" — %1 is dead").arg(q(npc_->name));
            } else if (!dc) {
                label += QString(" — %1: not while they're %2").arg(ask, dnd::attitude_name(npc_->attitude));
            } else if (*dc == 0) {
                label += QString(" — %1: no check needed").arg(ask);
            } else {
                label += QString(" — %1: DC %2").arg(ask).arg(*dc);
            }
        } else if (!option.check.empty()) {
            label += QString(" — %1, DC %2%3").arg(q(option.check)).arg(option.dc).arg(option.group ? " (everyone rolls)" : "");
        }
        auto* item = new QListWidgetItem(label, optionList_);
        if (!possible) item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    if (row < 0) {  // the first option that can be tried
        for (int i = 0; i < optionList_->count() && row < 0; ++i) {
            if (optionList_->item(i)->flags() & Qt::ItemIsEnabled) row = i;
        }
    }
    optionList_->setCurrentRow(row);
    optionList_->blockSignals(false);
    // Only as tall as its options, so what happened shows without scrolling.
    int rows = std::max(1, optionList_->count());
    optionList_->setFixedHeight(optionList_->sizeHintForRow(0) * rows + 2 * optionList_->frameWidth() + 4);
}

void SceneDialog::buildTrapOptions() {
    const auto& trap = scene_.trap;
    auto best = dnd::best_passive(party_, "Perception");
    if (best.score >= trap.spot_dc) {
        resultLabel_->setText(QString("<b>%1 spots %2</b> (passive Perception %3 vs DC %4).")
                                  .arg(q(best.name), q(trap.name))
                                  .arg(best.score)
                                  .arg(trap.spot_dc));
        dnd::SceneOption disarm;
        disarm.label = "Disarm it";
        disarm.check = trap.disarm_check;
        disarm.dc = trap.disarm_dc;
        disarm.success.text = "The trap is disarmed.";
        disarm.failure.text = "The trap goes off!";  // springs on whoever tried (see attempt())
        dnd::SceneOption around;
        around.label = "Go around it carefully";
        around.success.text = "The party gives it a wide berth.";
        options_ = {disarm, around};
        return;
    }
    // No one notices: it's sprung on whoever leads, at once.
    int victim = party_.empty() ? 0 : party_.front().character.character_id;
    QStringList lines;
    lines << QString("<b>No one notices %1</b> (best passive Perception %2 vs DC %3).")
                 .arg(q(trap.name))
                 .arg(best.score)
                 .arg(trap.spot_dc);
    for (const auto& line : dnd::spring_trap(repository_, trap, run_.party, victim, dice_)) lines << q(line);
    finish(lines);
}

SceneDialog::Ask SceneDialog::currentAsk() const {
    Ask ask;
    int row = optionList_->currentRow();
    if (row < 0 || row >= static_cast<int>(options_.size())) return ask;
    const dnd::SceneOption& option = options_[static_cast<size_t>(row)];
    ask.valid = true;
    if (option.social && npc_) {
        auto dc = dnd::conversation_dc(*npc_, option.request);
        ask.possible = dc.has_value() && !npc_->dead;
        if (dc && *dc > 0) {
            ask.check = approachCombo_->currentText().toStdString();
            ask.dc = *dc;
        }
        return ask;
    }
    ask.check = option.check;
    ask.dc = option.dc;
    ask.group = option.group;
    return ask;
}

void SceneDialog::refreshOption() {
    int row = optionList_->currentRow();
    {
        QSignalBlocker block(approachCombo_);
        QString keep = approachCombo_->currentText();
        approachCombo_->clear();
        if (row >= 0 && row < static_cast<int>(options_.size()) && options_[static_cast<size_t>(row)].social) {
            for (const auto& approach : dnd::social_approaches(options_[static_cast<size_t>(row)])) {
                approachCombo_->addItem(q(approach));
            }
        }
        approachCombo_->setCurrentIndex(std::max(0, approachCombo_->findText(keep)));
    }
    refreshRoller();
}

void SceneDialog::refreshRoller() {
    Ask ask = currentAsk();
    bool rolls = ask.valid && ask.possible && !ask.check.empty();
    // Reading the NPC rolls Insight, so the list shows it until a request is picked.
    std::string shown = rolls ? ask.check : (npc_ && !npc_->known && !npc_->dead && !readTried_ ? "Insight" : "");

    int keepRoller = rollerCombo_->currentData().toInt();
    rollerCombo_->blockSignals(true);
    rollerCombo_->clear();
    for (const auto& roller : party_) {
        QString label = q(roller.character.character_name);
        if (!shown.empty()) label += QString(" (%1)").arg(signedBonus(dnd::check_bonus(roller, shown)));
        rollerCombo_->addItem(label, roller.character.character_id);
    }
    rollerCombo_->setCurrentIndex(std::max(0, rollerCombo_->findData(keepRoller)));
    rollerCombo_->blockSignals(false);

    int roller = rollerCombo_->currentData().toInt();
    int keepHelper = helperCombo_->currentData().toInt();
    helperCombo_->clear();
    helperCombo_->addItem("No one", 0);
    for (const auto& member : party_) {
        if (member.character.character_id != roller) {
            helperCombo_->addItem(q(member.character.character_name), member.character.character_id);
        }
    }
    helperCombo_->setCurrentIndex(std::max(0, helperCombo_->findData(keepHelper)));
    bool inspired = false;
    for (const auto& member : party_) {
        if (member.character.character_id == roller) inspired = member.character.inspiration;
    }
    bool picksRoller = (rolls && !ask.group) || !shown.empty();
    for (QWidget* w : {static_cast<QWidget*>(rollerCombo_), static_cast<QWidget*>(helperCombo_)}) {
        w->setEnabled(picksRoller && !done_);
    }
    approachCombo_->setEnabled(rolls && !done_);
    inspirationBox_->setVisible(picksRoller && inspired);
    attemptButton_->setText(rolls ? (ask.group ? "Everyone rolls" : "Roll") : (npc_ ? "Ask" : "Choose"));
    attemptButton_->setEnabled(ask.valid && ask.possible && !done_);
}

void SceneDialog::readNpc() {
    if (!npc_ || readTried_ || done_) return;
    int roller_id = rollerCombo_->currentData().toInt();
    const dnd::CheckRoller* roller = nullptr;
    for (const auto& member : party_) {
        if (member.character.character_id == roller_id) roller = &member;
    }
    if (!roller) return;
    bool helped = helperCombo_->currentData().toInt() != 0;
    bool inspired = inspirationBox_->isVisible() && inspirationBox_->isChecked();
    if (inspired) {  // spent
        for (auto c : repository_.get_characters()) {
            if (c.character_id == roller_id) {
                c.inspiration = false;
                repository_.update_character(c);
            }
        }
        for (auto& member : party_) {
            if (member.character.character_id == roller_id) member.character.inspiration = false;
        }
        inspirationBox_->setChecked(false);
    }
    readTried_ = true;
    auto result = dnd::read_npc(repository_, *npc_, *roller, scene_.insight_dc, dice_, helped, inspired);
    QString line = q(result.text());
    line += result.success ? QString(". <b>You see what %1 wants, and what they fear.</b>").arg(q(npc_->name))
                           : QString(". %1 gives nothing away.").arg(q(npc_->name));
    resultLabel_->setText(line);
    refreshNpc();
    fillOptions();
    refreshOption();
}

void SceneDialog::attempt() {
    int row = optionList_->currentRow();
    Ask ask = currentAsk();
    if (!ask.valid || !ask.possible || done_) return;
    const dnd::SceneOption option = options_[static_cast<size_t>(row)];
    QStringList lines;
    int roller_id = rollerCombo_->currentData().toInt();
    bool success = true;

    if (ask.group) {
        auto result = dnd::group_check(dice_, party_, ask.check, ask.dc);
        for (const auto& roll : result.rolls) lines << q(roll.text());
        lines << (result.success ? "<b>The group succeeds.</b>" : "<b>The group fails.</b>");
        success = result.success;
        roller_id = 0;
    } else if (!ask.check.empty()) {
        const dnd::CheckRoller* roller = nullptr;
        for (const auto& member : party_) {
            if (member.character.character_id == roller_id) roller = &member;
        }
        if (!roller) return;
        bool helped = helperCombo_->currentData().toInt() != 0;
        bool inspired = inspirationBox_->isVisible() && inspirationBox_->isChecked();
        if (inspired) {  // spent
            for (auto c : repository_.get_characters()) {
                if (c.character_id == roller_id) {
                    c.inspiration = false;
                    repository_.update_character(c);
                }
            }
        }
        auto result = dnd::ability_check(dice_, *roller, ask.check, ask.dc, helped, inspired);
        lines << q(result.text());
        success = result.success;
    } else if (option.social && npc_) {
        lines << QString("%1 is %2: no check needed.").arg(q(npc_->name), dnd::attitude_name(npc_->attitude));
    }

    const dnd::SceneOutcome& outcome = success ? option.success : option.failure;
    for (const auto& line : dnd::apply_outcome(repository_, run_, outcome, roller_id, dice_, scene_.npc)) {
        lines << q(line);
    }
    // A trap that a failed disarm sets off.
    if (scene_.kind == dnd::SceneKind::Trap && !success && !option.check.empty()) {
        for (const auto& line : dnd::spring_trap(repository_, scene_.trap, run_.party, roller_id, dice_)) lines << q(line);
    }
    finish(lines);
}

void SceneDialog::finish(const QStringList& lines) {
    done_ = true;
    QString before = resultLabel_->text();
    resultLabel_->setText((before.isEmpty() ? QString() : before + "<br>") + lines.join("<br>"));
    dnd::record_scene(run_, adventure_);
    repository_.update_adventure_run(run_);
    optionList_->setEnabled(false);
    refreshRoller();
    if (npc_) {  // their attitude may have changed
        npc_ = dnd::find_run_npc(repository_, run_, scene_.npc);
        refreshNpc();
    }
    continueButton_->setEnabled(true);
}
