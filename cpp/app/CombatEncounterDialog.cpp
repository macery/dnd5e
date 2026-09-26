#include "CombatEncounterDialog.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "dnd/rules.hpp"

namespace {
QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}
}  // namespace

CombatEncounterDialog::CombatEncounterDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                                              std::vector<dnd::Monster> monsters, std::vector<dnd::Weapon> weapons,
                                              QWidget* parent)
    : QDialog(parent),
      repository_(repository),
      characters_(std::move(characters)),
      monsters_(std::move(monsters)),
      dice_(randomSource_) {
    setWindowTitle("Combat Encounter");

    statusLabel_ = new QLabel("Status: In Progress", this);
    statusLabel_->setStyleSheet("font-weight: bold;");

    participantTable_ = new QTableWidget(0, 5, this);
    participantTable_->setHorizontalHeaderLabels({"Name", "Initiative", "AC", "HP", "Status"});
    participantTable_->horizontalHeader()->setStretchLastSection(true);
    participantTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* monsterRow = new QHBoxLayout();
    monsterTypeCombo_ = new QComboBox(this);
    for (const auto& monster : monsters_) {
        monsterTypeCombo_->addItem(QString::fromStdString(monster.name), monster.monster_id);
    }
    monsterCountSpin_ = new QSpinBox(this);
    monsterCountSpin_->setRange(1, 20);
    monsterCountSpin_->setValue(1);
    addMonsterButton_ = new QPushButton("Add Monster", this);
    rollInitiativeButton_ = new QPushButton("Roll Initiative", this);
    monsterRow->addWidget(monsterTypeCombo_, 1);
    monsterRow->addWidget(monsterCountSpin_);
    monsterRow->addWidget(addMonsterButton_);
    monsterRow->addStretch(1);
    monsterRow->addWidget(rollInitiativeButton_);

    auto* attackGroup = new QGroupBox("Attack", this);
    auto* attackForm = new QFormLayout(attackGroup);
    attackerCombo_ = new QComboBox(attackGroup);
    targetCombo_ = new QComboBox(attackGroup);
    attackBonusSpin_ = new QSpinBox(attackGroup);
    attackBonusSpin_->setRange(-10, 20);
    damageDiceCountSpin_ = new QSpinBox(attackGroup);
    damageDiceCountSpin_->setRange(1, 20);
    damageDiceSidesSpin_ = new QSpinBox(attackGroup);
    damageDiceSidesSpin_->setRange(1, 100);
    damageBonusSpin_ = new QSpinBox(attackGroup);
    damageBonusSpin_->setRange(-10, 20);
    rollAttackButton_ = new QPushButton("Roll Attack", attackGroup);

    attackForm->addRow("Attacker", attackerCombo_);
    attackForm->addRow("Target", targetCombo_);
    attackForm->addRow("Attack Bonus", attackBonusSpin_);
    attackForm->addRow("Damage Dice Count", damageDiceCountSpin_);
    attackForm->addRow("Damage Dice Sides", damageDiceSidesSpin_);
    attackForm->addRow("Damage Bonus", damageBonusSpin_);
    attackForm->addRow(rollAttackButton_);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(statusLabel_);
    layout->addWidget(participantTable_, 1);
    layout->addLayout(monsterRow);
    layout->addWidget(attackGroup);
    layout->addWidget(new QLabel("Log", this));
    layout->addWidget(logView_, 1);
    resize(520, 640);

    connect(addMonsterButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onAddMonsterClicked);
    connect(rollInitiativeButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onRollInitiativeClicked);
    connect(attackerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CombatEncounterDialog::onAttackerChanged);
    connect(rollAttackButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onRollAttackClicked);

    addCharacterParticipants(weapons);
    refreshParticipantTable();
    refreshAttackCombos();
}

void CombatEncounterDialog::addCharacterParticipants(const std::vector<dnd::Weapon>& weapons) {
    for (const auto& character : characters_) {
        dnd::Weapon weapon;  // defaults to an unarmed-like fallback if not found
        for (const auto& candidate : weapons) {
            if (candidate.weapon_id == character.weapon_id) {
                weapon = candidate;
                break;
            }
        }

        Participant participant;
        participant.name = QString::fromStdString(character.character_name);
        participant.is_character = true;
        participant.character_id = character.character_id;
        participant.armor_class = character.armor_class;
        participant.current_hp = character.current_hp;
        participant.max_hp = character.max_hp;
        participant.dex_modifier = dnd::ability_modifier(character.dexterity);
        participant.attack_bonus = dnd::weapon_attack_bonus(character, weapon);
        participant.damage_dice_count = weapon.damage_dice_count;
        participant.damage_dice_sides = weapon.damage_dice_sides;
        participant.damage_bonus = dnd::weapon_attack_ability_modifier(character, weapon);
        participants_.push_back(participant);
    }
}

void CombatEncounterDialog::onAddMonsterClicked() {
    if (monsterTypeCombo_->count() == 0) return;

    int monster_id = monsterTypeCombo_->currentData().toInt();
    const dnd::Monster* monster = nullptr;
    for (const auto& candidate : monsters_) {
        if (candidate.monster_id == monster_id) monster = &candidate;
    }
    if (!monster) return;

    int count = monsterCountSpin_->value();
    for (int i = 1; i <= count; ++i) {
        Participant participant;
        participant.name = count > 1 ? QString("%1 #%2").arg(QString::fromStdString(monster->name)).arg(i)
                                      : QString::fromStdString(monster->name);
        participant.is_character = false;
        participant.armor_class = monster->armor_class;
        participant.current_hp = monster->hit_points;
        participant.max_hp = monster->hit_points;
        participant.dex_modifier = dnd::ability_modifier(monster->dexterity);
        participant.attack_bonus = monster->attack_bonus;
        participant.damage_dice_count = monster->damage_dice_count;
        participant.damage_dice_sides = monster->damage_dice_sides;
        participant.damage_bonus = monster->damage_bonus;
        participants_.push_back(participant);
    }

    refreshParticipantTable();
    refreshAttackCombos();
}

void CombatEncounterDialog::onRollInitiativeClicked() {
    for (auto& participant : participants_) {
        int d20 = dice_.roll(20, 1).front();
        participant.initiative = d20 + participant.dex_modifier;
    }
    refreshParticipantTable();
}

void CombatEncounterDialog::onAttackerChanged(int index) {
    if (index < 0 || index >= attackerCombo_->count()) return;

    int participant_index = attackerCombo_->itemData(index).toInt();
    const auto& attacker = participants_[static_cast<size_t>(participant_index)];
    attackBonusSpin_->setValue(attacker.attack_bonus);
    damageDiceCountSpin_->setValue(attacker.damage_dice_count);
    damageDiceSidesSpin_->setValue(attacker.damage_dice_sides);
    damageBonusSpin_->setValue(attacker.damage_bonus);
}

void CombatEncounterDialog::onRollAttackClicked() {
    if (attackerCombo_->count() == 0 || targetCombo_->count() == 0) return;

    int attacker_index = attackerCombo_->currentData().toInt();
    int target_index = targetCombo_->currentData().toInt();
    const Participant& attacker = participants_[static_cast<size_t>(attacker_index)];
    Participant& target = participants_[static_cast<size_t>(target_index)];

    auto result = dnd::resolve_attack(dice_, attackBonusSpin_->value(), target.armor_class,
                                       damageDiceCountSpin_->value(), damageDiceSidesSpin_->value(),
                                       damageBonusSpin_->value());

    QString outcome;
    if (!result.hit) {
        outcome = "miss";
    } else {
        target.current_hp = std::max(0, target.current_hp - result.damage_total);
        outcome = result.critical ? QString("CRITICAL HIT for %1").arg(result.damage_total)
                                   : QString("hit for %1").arg(result.damage_total);
    }

    log(QString("%1 attacks %2: d20(%3) %4 -> %5. %2 is now %6/%7 HP.")
            .arg(attacker.name, target.name)
            .arg(result.attack_roll)
            .arg(formatModifier(attackBonusSpin_->value()), outcome)
            .arg(target.current_hp)
            .arg(target.max_hp));

    if (target.is_character) {
        for (auto& character : characters_) {
            if (character.character_id == target.character_id) {
                character.current_hp = target.current_hp;
                repository_.update_character(character);
                break;
            }
        }
    }

    refreshParticipantTable();
    checkForVictoryOrDefeat();
}

void CombatEncounterDialog::checkForVictoryOrDefeat() {
    if (encounterOver_) return;

    bool has_characters = false;
    bool has_monsters = false;
    bool all_characters_down = true;
    bool all_monsters_down = true;

    for (const auto& participant : participants_) {
        bool down = participant.current_hp <= 0;
        if (participant.is_character) {
            has_characters = true;
            all_characters_down = all_characters_down && down;
        } else {
            has_monsters = true;
            all_monsters_down = all_monsters_down && down;
        }
    }

    if (has_characters && all_characters_down) {
        encounterOver_ = true;
        statusLabel_->setText("Status: Defeat! All characters have fallen.");
        log("--- Defeat! All characters have fallen. ---");
    } else if (has_monsters && all_monsters_down) {
        encounterOver_ = true;
        statusLabel_->setText("Status: Victory! All enemies defeated.");
        log("--- Victory! All enemies defeated. ---");
    }

    if (encounterOver_) rollAttackButton_->setEnabled(false);
}

void CombatEncounterDialog::refreshParticipantTable() {
    std::vector<int> order(participants_.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(),
              [this](int a, int b) { return participants_[static_cast<size_t>(a)].initiative >
                                             participants_[static_cast<size_t>(b)].initiative; });

    participantTable_->setRowCount(static_cast<int>(order.size()));
    for (int row = 0; row < static_cast<int>(order.size()); ++row) {
        const auto& participant = participants_[static_cast<size_t>(order[static_cast<size_t>(row)])];
        participantTable_->setItem(row, 0, new QTableWidgetItem(participant.name));
        participantTable_->setItem(row, 1, new QTableWidgetItem(QString::number(participant.initiative)));
        participantTable_->setItem(row, 2, new QTableWidgetItem(QString::number(participant.armor_class)));
        participantTable_->setItem(
            row, 3, new QTableWidgetItem(QString("%1/%2").arg(participant.current_hp).arg(participant.max_hp)));
        participantTable_->setItem(row, 4, new QTableWidgetItem(participant.current_hp > 0 ? "Up" : "Down"));
    }
}

void CombatEncounterDialog::refreshAttackCombos() {
    attackerCombo_->blockSignals(true);
    attackerCombo_->clear();
    targetCombo_->clear();
    for (int i = 0; i < static_cast<int>(participants_.size()); ++i) {
        attackerCombo_->addItem(participants_[static_cast<size_t>(i)].name, i);
        targetCombo_->addItem(participants_[static_cast<size_t>(i)].name, i);
    }
    attackerCombo_->blockSignals(false);
    if (!participants_.empty()) onAttackerChanged(0);
}

void CombatEncounterDialog::log(const QString& line) {
    logView_->appendPlainText(line);
}
