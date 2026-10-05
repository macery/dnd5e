#include "SceneDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
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

    auto* narration = new QLabel(QString("<b>%1.</b> %2").arg(q(scene_.title).toHtmlEscaped(),
                                                            q(scene_.narration).toHtmlEscaped()),
                                 this);
    narration->setWordWrap(true);

    optionList_ = new QListWidget(this);
    rollerCombo_ = new QComboBox(this);
    helperCombo_ = new QComboBox(this);
    helperCombo_->setToolTip("Help (PHB): another character who could do it too gives advantage on the check.");
    inspirationBox_ = new QCheckBox("Use inspiration (advantage)", this);
    attemptButton_ = new QPushButton("Roll", this);
    resultLabel_ = new QLabel(this);
    resultLabel_->setWordWrap(true);

    auto* form = new QFormLayout();
    form->addRow("Who", rollerCombo_);
    form->addRow("Helped by", helperCombo_);
    form->addRow("", inspirationBox_);

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
    contentLayout->addWidget(optionList_);
    contentLayout->addLayout(form);
    contentLayout->addWidget(attemptButton_);
    contentLayout->addWidget(resultLabel_);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scrollable(content, this), 1);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(560, 520), "Scene");

    connect(optionList_, &QListWidget::currentRowChanged, this, [this](int) { refreshRoller(); });
    connect(rollerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshRoller(); });
    connect(attemptButton_, &QPushButton::clicked, this, &SceneDialog::attempt);

    if (scene_.kind == dnd::SceneKind::Trap) {
        buildTrapOptions();
    } else {
        for (const dnd::SceneOption* option : dnd::available_options(scene_, run_)) options_.push_back(*option);
    }
    for (const auto& option : options_) {
        QString label = q(option.label);
        if (!option.check.empty()) {
            label += QString(" — %1, DC %2%3").arg(q(option.check)).arg(option.dc).arg(option.group ? " (everyone rolls)" : "");
        }
        optionList_->addItem(label);
    }
    if (optionList_->count() > 0) optionList_->setCurrentRow(0);
    refreshRoller();
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

void SceneDialog::refreshRoller() {
    int row = optionList_->currentRow();
    bool rolls = row >= 0 && row < static_cast<int>(options_.size()) && !options_[static_cast<size_t>(row)].check.empty();
    bool group = rolls && options_[static_cast<size_t>(row)].group;
    std::string check = rolls ? options_[static_cast<size_t>(row)].check : "";

    int keepRoller = rollerCombo_->currentData().toInt();
    rollerCombo_->blockSignals(true);
    rollerCombo_->clear();
    for (const auto& roller : party_) {
        QString label = q(roller.character.character_name);
        if (rolls) label += QString(" (%1)").arg(signedBonus(dnd::check_bonus(roller, check)));
        rollerCombo_->addItem(label, roller.character.character_id);
    }
    rollerCombo_->setCurrentIndex(std::max(0, rollerCombo_->findData(keepRoller)));
    rollerCombo_->blockSignals(false);

    int roller = rollerCombo_->currentData().toInt();
    helperCombo_->clear();
    helperCombo_->addItem("No one", 0);
    for (const auto& member : party_) {
        if (member.character.character_id != roller) {
            helperCombo_->addItem(q(member.character.character_name), member.character.character_id);
        }
    }
    bool inspired = false;
    for (const auto& member : party_) {
        if (member.character.character_id == roller) inspired = member.character.inspiration;
    }
    for (QWidget* w : {static_cast<QWidget*>(rollerCombo_), static_cast<QWidget*>(helperCombo_)}) {
        w->setEnabled(rolls && !group && !done_);
    }
    inspirationBox_->setVisible(rolls && !group && inspired);
    attemptButton_->setText(rolls ? (group ? "Everyone rolls" : "Roll") : "Choose");
    attemptButton_->setEnabled(row >= 0 && !done_);
}

void SceneDialog::attempt() {
    int row = optionList_->currentRow();
    if (row < 0 || row >= static_cast<int>(options_.size()) || done_) return;
    const dnd::SceneOption option = options_[static_cast<size_t>(row)];
    QStringList lines;
    int roller_id = rollerCombo_->currentData().toInt();
    bool success = true;

    if (option.group) {
        auto result = dnd::group_check(dice_, party_, option.check, option.dc);
        for (const auto& roll : result.rolls) lines << q(roll.text());
        lines << (result.success ? "<b>The group succeeds.</b>" : "<b>The group fails.</b>");
        success = result.success;
        roller_id = 0;
    } else if (!option.check.empty()) {
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
        auto result = dnd::ability_check(dice_, *roller, option.check, option.dc, helped, inspired);
        lines << q(result.text());
        success = result.success;
    }

    const dnd::SceneOutcome& outcome = success ? option.success : option.failure;
    for (const auto& line : dnd::apply_outcome(repository_, run_, outcome, roller_id, dice_)) lines << q(line);
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
    continueButton_->setEnabled(true);
}
