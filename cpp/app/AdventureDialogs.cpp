#include "AdventureDialogs.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "LevelUpDialog.hpp"
#include "WindowSizing.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

QString classNameOf(const std::vector<dnd::LookupEntry>& classes, int class_id) {
    for (const auto& entry : classes) {
        if (entry.id == class_id) return q(entry.name);
    }
    return QString();
}

// "Rage 1/3 · Second Wind 0/1 · Slots L1 1/2" -- what's left of the limited resources.
QString resourcesText(const dnd::Character& character, const std::vector<dnd::LookupEntry>& classes) {
    QStringList parts;
    std::string class_name = classNameOf(classes, character.class_id).toStdString();
    for (const auto& info : dnd::limited_features()) {
        if (!dnd::has_feature(info.feature, class_name, character.level)) continue;
        int max = dnd::feature_max_uses(info.feature, character, class_name);
        if (max >= 99 || info.feature == dnd::Feature::ArcaneRecovery) continue;
        parts << QString("%1 %2/%3").arg(q(info.name)).arg(dnd::feature_uses_left(info.feature, character, class_name)).arg(max);
    }
    if (const dnd::ClassInfo* info = dnd::find_class_info_by_id(classes, character.class_id)) {
        QStringList slotParts;  // not `slots`: that's a Qt macro
        auto totals = dnd::spell_slots(*info, character.level);
        for (int level = 1; level <= dnd::kMaxSpellLevel; ++level) {
            int total = totals[static_cast<size_t>(level - 1)];
            if (total > 0) slotParts << QString("L%1 %2/%3").arg(level).arg(dnd::slots_remaining(character, *info, level)).arg(total);
        }
        if (!slotParts.isEmpty()) parts << "Slots " + slotParts.join(" ");
    }
    return parts.isEmpty() ? QString("—") : parts.join(" · ");
}

}  // namespace

// --- PartyDialog ----------------------------------------------------------------------

PartyDialog::PartyDialog(const std::vector<dnd::Character>& living, const std::vector<dnd::Player>& players,
                         const std::vector<dnd::LookupEntry>& classes, int default_player_id,
                         const QString& adventure, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Choose the Party");
    auto* intro = new QLabel(QString("Who sets out on <b>%1</b>? Monsters are scaled for the party you choose.")
                                 .arg(adventure.toHtmlEscaped()),
                             this);
    intro->setWordWrap(true);

    list_ = new QListWidget(this);
    for (const auto& character : living) {
        QString player;
        for (const auto& p : players) {
            if (p.player_id == character.player_id) player = q(p.player_name);
        }
        auto* item = new QListWidgetItem(QString("%1 — level %2 %3 (%4) · %5/%6 HP")
                                             .arg(q(character.character_name))
                                             .arg(character.level)
                                             .arg(classNameOf(classes, character.class_id), player)
                                             .arg(character.current_hp)
                                             .arg(character.max_hp),
                                         list_);
        item->setData(Qt::UserRole, character.character_id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(character.player_id == default_player_id ? Qt::Checked : Qt::Unchecked);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* go = buttons->addButton("Set Out", QDialogButtonBox::AcceptRole);
    auto refreshGo = [this, go] { go->setEnabled(!partyIds().empty()); };
    connect(list_, &QListWidget::itemChanged, this, refreshGo);
    refreshGo();
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(list_, 1);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(480, 360), "Party");
}

std::vector<int> PartyDialog::partyIds() const {
    std::vector<int> ids;
    for (int row = 0; row < list_->count(); ++row) {
        if (list_->item(row)->checkState() == Qt::Checked) ids.push_back(list_->item(row)->data(Qt::UserRole).toInt());
    }
    return ids;
}

// --- ShortRestDialog -------------------------------------------------------------------

ShortRestDialog::ShortRestDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                                 const std::vector<dnd::LookupEntry>& classes, QWidget* parent)
    : QDialog(parent), repository_(repository), characters_(std::move(characters)), classes_(classes) {
    setWindowTitle("Short Rest");
    auto* intro = new QLabel(
        "An hour's rest. Each character can spend hit dice: roll each one and add your Constitution modifier "
        "to regain HP. Short-rest features (Second Wind, Action Surge, Ki, ...), a Warlock's slots, and a "
        "Wizard's Arcane Recovery come back too. Spent hit dice return on a long rest (half your level).",
        this);
    intro->setWordWrap(true);

    auto* table = new QTableWidget(static_cast<int>(characters_.size()), 4, this);
    table->setHorizontalHeaderLabels({"Character", "HP", "Hit dice left", "Spend"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    for (int row = 0; row < static_cast<int>(characters_.size()); ++row) {
        const auto& c = characters_[static_cast<size_t>(row)];
        int die = dnd::hit_die_for_class_id(classes_, c.class_id);
        int left = dnd::hit_dice_remaining(c);
        table->setItem(row, 0, new QTableWidgetItem(q(c.character_name)));
        table->setItem(row, 1, new QTableWidgetItem(QString("%1/%2").arg(c.current_hp).arg(c.max_hp)));
        table->setItem(row, 2, new QTableWidgetItem(QString("%1 × d%2").arg(left).arg(die)));
        auto* spin = new QSpinBox(table);
        spin->setRange(0, c.dead ? 0 : left);
        // Enough dice, on average, to get back to full.
        double average = die / 2.0 + 0.5 + dnd::ability_modifier(c.constitution);
        int needed = average > 0 ? static_cast<int>(std::ceil((c.max_hp - c.current_hp) / average)) : 0;
        spin->setValue(std::clamp(needed, 0, spin->maximum()));
        spin->setToolTip(QString("Each die: 1d%1 %2 CON.").arg(die).arg(dnd::ability_modifier(c.constitution) >= 0
                                                                            ? QString("+ %1").arg(dnd::ability_modifier(c.constitution))
                                                                            : QString("− %1").arg(-dnd::ability_modifier(c.constitution))));
        table->setCellWidget(row, 3, spin);
        spins_.push_back(spin);
    }

    resultLabel_ = new QLabel(this);
    resultLabel_->setWordWrap(true);
    resultLabel_->hide();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    restButton_ = buttons->addButton("Rest", QDialogButtonBox::ActionRole);
    connect(restButton_, &QPushButton::clicked, this, [this, buttons] {
        rest();
        restButton_->setEnabled(false);
        buttons->button(QDialogButtonBox::Cancel)->setText("Close");
    });
    connect(buttons, &QDialogButtonBox::rejected, this, [this] { rested_ ? accept() : reject(); });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(table, 1);
    layout->addWidget(resultLabel_);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(520, 360), "ShortRest");
}

void ShortRestDialog::rest() {
    dnd::SystemRandomSource source;
    dnd::Dice dice(source);
    for (size_t i = 0; i < characters_.size(); ++i) {
        auto& c = characters_[i];
        const dnd::ClassInfo* info = dnd::find_class_info_by_id(classes_, c.class_id);
        if (!info || c.dead) continue;
        auto result = dnd::short_rest(c, *info, dice, spins_[i]->value());
        repository_.update_character(c);
        QStringList what;
        if (result.hit_dice_spent > 0) {
            what << QString("spends %1 hit %2, regains %3 HP (now %4/%5)")
                        .arg(result.hit_dice_spent)
                        .arg(result.hit_dice_spent == 1 ? "die" : "dice")
                        .arg(result.hp_regained)
                        .arg(c.current_hp)
                        .arg(c.max_hp);
        }
        if (result.features_restored) what << "short-rest features back";
        if (result.slots_restored) what << "pact slots back";
        QStringList recovered;
        for (int level = 1; level <= 9; ++level) {
            int count = result.arcane_recovery[static_cast<size_t>(level - 1)];
            if (count > 0) recovered << QString("%1 × level %2").arg(count).arg(level);
        }
        if (!recovered.isEmpty()) what << "Arcane Recovery: " + recovered.join(", ");
        summary_ << q(c.character_name) + ": " + (what.isEmpty() ? QString("rests") : what.join("; ")) + ".";
    }
    rested_ = true;
    resultLabel_->setText(summary_.join("\n"));
    resultLabel_->show();
}

// --- BetweenScenesDialog ------------------------------------------------------------------

BetweenScenesDialog::BetweenScenesDialog(dnd::Repository& repository, dnd::AdventureRun& run,
                                         const dnd::AdventureTemplate& adventure,
                                         const std::vector<dnd::LookupEntry>& classes, QWidget* parent)
    : QDialog(parent), repository_(repository), run_(run), adventure_(adventure), classes_(classes) {
    setWindowTitle(q(adventure.title));
    const dnd::AdventureScene* next = dnd::next_scene(run_, adventure_);
    auto* header = new QLabel(this);
    header->setWordWrap(true);
    header->setText(QString("<b>%1 of %2 fights won.</b> Next: <b>%3</b> (%4)%5")
                        .arg(run_.next_scene)
                        .arg(adventure_.scenes.size())
                        .arg(next ? q(next->title).toHtmlEscaped() : QString())
                        .arg(q(dnd::progress_text(run_, adventure_)))
                        .arg(next && next->boss ? " — <i>the final fight</i>" : ""));

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Character", "HP", "Hit dice", "Resources left"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);

    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);

    auto* pressOn = new QPushButton("Press On", this);
    pressOn->setToolTip("Into the next fight as you are.");
    auto* shortRest = new QPushButton("Short Rest…", this);
    shortRest->setToolTip("An hour: spend hit dice to heal; short-rest features come back. As many as you like.");
    longRestButton_ = new QPushButton("Long Rest", this);
    auto* retreatButton = new QPushButton("Retreat", this);
    retreatButton->setToolTip("Leave the adventure, keeping the XP, coins, and loot earned so far.");
    auto* row = new QHBoxLayout();
    row->addWidget(pressOn);
    row->addWidget(shortRest);
    row->addWidget(longRestButton_);
    row->addStretch(1);
    row->addWidget(retreatButton);

    auto* note = new QLabel("Closing this window stops for now: the adventure is saved, and Solo Adventure offers "
                            "to resume it.",
                            this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #606060;");

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(header);
    layout->addWidget(table_, 1);
    layout->addWidget(messageLabel_);
    layout->addLayout(row);
    layout->addWidget(note);
    fitToScreen(this, QSize(640, 380), "BetweenScenes");

    connect(pressOn, &QPushButton::clicked, this, [this] {
        choice_ = Choice::PressOn;
        accept();
    });
    connect(retreatButton, &QPushButton::clicked, this, [this] {
        choice_ = Choice::Retreat;
        accept();
    });
    connect(shortRest, &QPushButton::clicked, this, &BetweenScenesDialog::onShortRest);
    connect(longRestButton_, &QPushButton::clicked, this, &BetweenScenesDialog::onLongRest);
    refresh();
}

std::vector<dnd::Character> BetweenScenesDialog::party() const {
    std::vector<dnd::Character> members;
    for (const auto& character : repository_.get_characters()) {
        bool inParty = std::find(run_.party.begin(), run_.party.end(), character.character_id) != run_.party.end();
        if (inParty && !character.dead) members.push_back(character);
    }
    return members;
}

void BetweenScenesDialog::refresh() {
    auto members = party();
    table_->setRowCount(static_cast<int>(members.size()));
    for (int row = 0; row < static_cast<int>(members.size()); ++row) {
        const auto& c = members[static_cast<size_t>(row)];
        QString name = q(c.character_name);
        if (dnd::pending_levels(c) > 0) name += " (level up ready)";
        table_->setItem(row, 0, new QTableWidgetItem(name));
        auto* hp = new QTableWidgetItem(QString("%1/%2").arg(c.current_hp).arg(c.max_hp));
        if (c.current_hp <= 0) hp->setForeground(QColor(176, 0, 32));
        table_->setItem(row, 1, hp);
        table_->setItem(row, 2, new QTableWidgetItem(QString("%1/%2 (d%3)")
                                                         .arg(dnd::hit_dice_remaining(c))
                                                         .arg(c.level)
                                                         .arg(dnd::hit_die_for_class_id(classes_, c.class_id))));
        table_->setItem(row, 3, new QTableWidgetItem(resourcesText(c, classes_)));
    }
    bool camp = dnd::can_long_rest(run_, adventure_);
    longRestButton_->setEnabled(camp);
    longRestButton_->setToolTip(camp ? "Make camp for the night: full HP, spell slots, and features, half your hit "
                                       "dice back -- and a chance to level up. Once per camp."
                                     : run_.long_rest_taken ? "You've already rested at this camp."
                                                            : "There's nowhere safe to make camp here. (This adventure "
                                                              "has camps at set points.)");
}

void BetweenScenesDialog::onShortRest() {
    ShortRestDialog dialog(repository_, party(), classes_, this);
    dialog.exec();
    if (dialog.rested()) messageLabel_->setText(dialog.summary().join("\n"));
    refresh();
}

void BetweenScenesDialog::onLongRest() {
    if (!dnd::can_long_rest(run_, adventure_)) return;
    QStringList lines;
    for (auto character : party()) {
        dnd::take_long_rest(character);
        repository_.update_character(character);
    }
    dnd::record_long_rest(run_);
    repository_.update_adventure_run(run_);
    lines << "The party makes camp and sleeps through the night: HP, spell slots, and features restored.";

    // Anyone whose XP has earned a level can take it now (decision 4).
    for (const auto& member : party()) {
        dnd::Character character = member;
        while (dnd::pending_levels(character) > 0) {
            LevelUpDialog levelUp(repository_, character, classes_, this);
            if (levelUp.exec() != QDialog::Accepted) break;
            character = levelUp.character();
            lines << QString("%1 reaches level %2.").arg(q(character.character_name)).arg(character.level);
        }
    }
    messageLabel_->setText(lines.join("\n"));
    refresh();
}
