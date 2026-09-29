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

#include "dnd/inventory.hpp"
#include "dnd/progression.hpp"
#include "dnd/rules.hpp"

namespace {
QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}
}  // namespace

CombatEncounterDialog::CombatEncounterDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                                              std::vector<dnd::Monster> monsters, std::vector<dnd::Weapon> weapons,
                                              QWidget* parent, const dnd::AdventureTemplate* adventure)
    : QDialog(parent),
      repository_(repository),
      characters_(std::move(characters)),
      monsters_(std::move(monsters)),
      classes_(repository.get_classes()),
      dice_(randomSource_) {
    setWindowTitle("Combat Encounter");

    statusLabel_ = new QLabel("Status: In Progress", this);
    statusLabel_->setStyleSheet("font-weight: bold;");

    if (adventure) {
        victoryText_ = QString::fromStdString(adventure->victory_text);
        loot_ = adventure->loot;
        objectiveLabel_ = new QLabel(
            "Objective: " + QString::fromStdString(adventure->hook), this);
        objectiveLabel_->setWordWrap(true);
    }

    participantTable_ = new QTableWidget(0, 5, this);
    participantTable_->setHorizontalHeaderLabels({"Name", "Initiative", "AC", "HP", "Status"});
    participantTable_->horizontalHeader()->setStretchLastSection(true);
    participantTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    participantTable_->horizontalHeaderItem(1)->setToolTip(
        "Turn order for this encounter, highest first. Rolled once at the start of combat.");
    participantTable_->horizontalHeaderItem(2)->setToolTip(
        "Armor Class -- how hard this participant is to hit. An attack roll must meet or beat this to hit.");
    participantTable_->horizontalHeaderItem(3)->setToolTip("Current hit points out of maximum.");
    participantTable_->horizontalHeaderItem(4)->setToolTip(
        "\"Dodging\" means attacks against this participant have disadvantage until the start of their "
        "next turn.");

    auto* monsterRow = new QHBoxLayout();
    monsterTypeCombo_ = new QComboBox(this);
    for (const auto& monster : monsters_) {
        monsterTypeCombo_->addItem(QString::fromStdString(monster.name), monster.monster_id);
    }
    monsterCountSpin_ = new QSpinBox(this);
    monsterCountSpin_->setRange(1, 20);
    monsterCountSpin_->setValue(1);
    addMonsterButton_ = new QPushButton("Add Monster", this);
    monsterRow->addWidget(monsterTypeCombo_, 1);
    monsterRow->addWidget(monsterCountSpin_);
    monsterRow->addWidget(addMonsterButton_);

    auto* turnRow = new QHBoxLayout();
    rollInitiativeButton_ = new QPushButton("Roll Initiative", this);
    nextTurnButton_ = new QPushButton("Next Turn", this);
    currentTurnLabel_ = new QLabel("Current Turn: -", this);
    currentTurnLabel_->setStyleSheet("font-weight: bold;");
    turnRow->addWidget(rollInitiativeButton_);
    turnRow->addWidget(nextTurnButton_);
    turnRow->addStretch(1);
    turnRow->addWidget(currentTurnLabel_);

    auto* attackGroup = new QGroupBox("Your Turn", this);
    auto* attackForm = new QFormLayout(attackGroup);
    attackFormLayout_ = attackForm;

    moveLabel_ = new QLabel("Movement: up to 30 ft. this turn (not tracked -- no map)", attackGroup);
    actionCombo_ = new QComboBox(attackGroup);
    actionCombo_->addItem("Attack");
    actionCombo_->setItemData(0, "Make one weapon attack against a target.", Qt::ToolTipRole);
    actionCombo_->addItem("Dash");
    actionCombo_->setItemData(
        1, "Double your movement for the turn (informational only -- there's no map to move on).",
        Qt::ToolTipRole);
    actionCombo_->addItem("Dodge");
    actionCombo_->setItemData(2,
        "Until the start of your next turn, attacks against you have disadvantage.", Qt::ToolTipRole);
    actionCombo_->addItem("Drink Potion");
    actionCombo_->setItemData(3, "Drink a healing potion from your inventory to regain hit points.",
                              Qt::ToolTipRole);
    targetCombo_ = new QComboBox(attackGroup);
    potionCombo_ = new QComboBox(attackGroup);
    attackBonusSpin_ = new QSpinBox(attackGroup);
    attackBonusSpin_->setRange(-10, 20);
    damageDiceCountSpin_ = new QSpinBox(attackGroup);
    damageDiceCountSpin_->setRange(1, 20);
    damageDiceSidesSpin_ = new QSpinBox(attackGroup);
    damageDiceSidesSpin_->setRange(1, 100);
    damageBonusSpin_ = new QSpinBox(attackGroup);
    damageBonusSpin_->setRange(-10, 20);
    bonusActionLabel_ = new QLabel("Bonus Action: none available", attackGroup);
    takeActionButton_ = new QPushButton("Roll Attack", attackGroup);
    takeActionButton_->setEnabled(false);

    attackForm->addRow(moveLabel_);
    attackForm->addRow("Action", actionCombo_);
    attackForm->addRow("Target", targetCombo_);
    attackForm->addRow("Potion", potionCombo_);
    attackForm->addRow("Attack Bonus", attackBonusSpin_);
    attackForm->addRow("Damage Dice Count", damageDiceCountSpin_);
    attackForm->addRow("Damage Dice Sides", damageDiceSidesSpin_);
    attackForm->addRow("Damage Bonus", damageBonusSpin_);
    attackForm->addRow(bonusActionLabel_);
    attackForm->addRow(takeActionButton_);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(statusLabel_);
    if (objectiveLabel_) layout->addWidget(objectiveLabel_);
    layout->addWidget(participantTable_, 1);
    layout->addLayout(monsterRow);
    layout->addLayout(turnRow);
    layout->addWidget(attackGroup);
    layout->addWidget(new QLabel("Log", this));
    layout->addWidget(logView_, 1);
    resize(520, 700);

    connect(addMonsterButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onAddMonsterClicked);
    connect(rollInitiativeButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onRollInitiativeClicked);
    connect(nextTurnButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onNextTurnClicked);
    connect(actionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CombatEncounterDialog::onActionChanged);
    connect(takeActionButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onTakeActionClicked);

    updateActionControlsVisibility();
    addCharacterParticipants(weapons);

    if (adventure) {
        for (const auto& group : adventure->monster_groups) {
            for (const auto& candidate : monsters_) {
                if (candidate.name == group.monster_name) {
                    addMonsterParticipants(candidate, group.count);
                    break;
                }
            }
        }
    }

    refreshParticipantTable();
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

void CombatEncounterDialog::addMonsterParticipants(const dnd::Monster& monster, int count) {
    for (int i = 1; i <= count; ++i) {
        Participant participant;
        participant.name = count > 1 ? QString("%1 #%2").arg(QString::fromStdString(monster.name)).arg(i)
                                      : QString::fromStdString(monster.name);
        participant.is_character = false;
        participant.armor_class = monster.armor_class;
        participant.current_hp = monster.hit_points;
        participant.max_hp = monster.hit_points;
        participant.dex_modifier = dnd::ability_modifier(monster.dexterity);
        participant.attack_bonus = monster.attack_bonus;
        participant.damage_dice_count = monster.damage_dice_count;
        participant.damage_dice_sides = monster.damage_dice_sides;
        participant.damage_bonus = monster.damage_bonus;
        participant.xp = dnd::xp_for_challenge_rating(monster.challenge_rating);
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
    addMonsterParticipants(*monster, count);

    log(QString("%1 x%2 joins the encounter.").arg(QString::fromStdString(monster->name)).arg(count) +
        (turnOrder_.empty() ? "" : " (click Roll Initiative to include them in turn order)"));

    refreshParticipantTable();
}

void CombatEncounterDialog::onRollInitiativeClicked() {
    for (auto& participant : participants_) {
        int d20 = dice_.roll(20, 1).front();
        participant.initiative = d20 + participant.dex_modifier;
    }

    turnOrder_.resize(participants_.size());
    for (size_t i = 0; i < turnOrder_.size(); ++i) turnOrder_[i] = static_cast<int>(i);
    std::sort(turnOrder_.begin(), turnOrder_.end(), [this](int a, int b) {
        return participants_[static_cast<size_t>(a)].initiative > participants_[static_cast<size_t>(b)].initiative;
    });
    currentTurnIndex_ = turnOrder_.empty() ? -1 : 0;

    log("--- Initiative rolled. Combat begins! ---");
    refreshParticipantTable();
    advanceToActionableTurn();
}

void CombatEncounterDialog::onNextTurnClicked() {
    if (turnOrder_.empty() || encounterOver_ || currentTurnIndex_ < 0) return;
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
    advanceToActionableTurn();
}

void CombatEncounterDialog::advanceToActionableTurn() {
    if (turnOrder_.empty()) return;

    // Bounded to one full lap: checkForVictoryOrDefeat() should always catch
    // "everyone on one side is down" before this could loop forever, but the
    // bound is a cheap defensive guard against that invariant ever slipping.
    for (size_t guard = 0; guard < turnOrder_.size() && !encounterOver_; ++guard) {
        Participant& current = participants_[static_cast<size_t>(turnOrder_[static_cast<size_t>(currentTurnIndex_)])];

        if (current.current_hp <= 0) {
            currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
            continue;
        }

        // Dodge protects only until the start of this participant's own next
        // turn, which is exactly now.
        current.is_dodging = false;

        if (!current.is_character) {
            resolveMonsterTurn(current);
            refreshParticipantTable();
            checkForVictoryOrDefeat();
            if (encounterOver_) break;
            currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
            continue;
        }

        // A living character's turn -- stop here and let the user act.
        currentTurnLabel_->setText("Current Turn: " + current.name);
        actionCombo_->setCurrentIndex(0);
        updateActionControlsVisibility();
        refreshTargetCombo();
        refreshPotionCombo();
        updateAttackFieldsForCurrentTurn();
        takeActionButton_->setEnabled(true);
        return;
    }

    if (encounterOver_) {
        currentTurnLabel_->setText("Current Turn: -");
        targetCombo_->clear();
        takeActionButton_->setEnabled(false);
    }
}

void CombatEncounterDialog::resolveMonsterTurn(Participant& monster) {
    std::vector<int> living_indices;
    for (int i = 0; i < static_cast<int>(participants_.size()); ++i) {
        const auto& candidate = participants_[static_cast<size_t>(i)];
        if (candidate.is_character && candidate.current_hp > 0) living_indices.push_back(i);
    }
    if (living_indices.empty()) return;  // shouldn't happen; win/loss would already have caught it

    int pick = dice_.roll(static_cast<int>(living_indices.size()), 1).front() - 1;
    Participant& target = participants_[static_cast<size_t>(living_indices[static_cast<size_t>(pick)])];

    auto mode = target.is_dodging ? dnd::RollMode::Disadvantage : dnd::RollMode::Normal;
    auto result = dnd::resolve_attack(dice_, monster.attack_bonus, target.armor_class, monster.damage_dice_count,
                                       monster.damage_dice_sides, monster.damage_bonus, mode);
    QString outcome = applyDamage(target, result);

    log(QString("%1 attacks %2: d20(%3) %4 -> %5. %2 is now %6/%7 HP.")
            .arg(monster.name, target.name)
            .arg(result.attack_roll)
            .arg(formatModifier(monster.attack_bonus), outcome)
            .arg(target.current_hp)
            .arg(target.max_hp));
}

void CombatEncounterDialog::onActionChanged(int /*index*/) {
    updateActionControlsVisibility();
}

void CombatEncounterDialog::updateActionControlsVisibility() {
    bool is_attack = actionCombo_->currentIndex() == 0;
    targetCombo_->setVisible(is_attack);
    attackBonusSpin_->setVisible(is_attack);
    damageDiceCountSpin_->setVisible(is_attack);
    damageDiceSidesSpin_->setVisible(is_attack);
    damageBonusSpin_->setVisible(is_attack);
    for (auto* field : {static_cast<QWidget*>(targetCombo_), static_cast<QWidget*>(attackBonusSpin_),
                         static_cast<QWidget*>(damageDiceCountSpin_), static_cast<QWidget*>(damageDiceSidesSpin_),
                         static_cast<QWidget*>(damageBonusSpin_)}) {
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(is_attack);
    }
    bool is_potion = actionCombo_->currentIndex() == 3;
    potionCombo_->setVisible(is_potion);
    if (auto* row_label = attackFormLayout_->labelForField(potionCombo_)) row_label->setVisible(is_potion);

    switch (actionCombo_->currentIndex()) {
        case 1: takeActionButton_->setText("Take Dash Action"); break;
        case 2: takeActionButton_->setText("Take Dodge Action"); break;
        case 3: takeActionButton_->setText("Drink Potion"); break;
        default: takeActionButton_->setText("Roll Attack"); break;
    }
}

void CombatEncounterDialog::onTakeActionClicked() {
    if (currentTurnIndex_ < 0 || turnOrder_.empty()) return;
    Participant& actor = participants_[static_cast<size_t>(turnOrder_[static_cast<size_t>(currentTurnIndex_)])];

    switch (actionCombo_->currentIndex()) {
        case 1:  // Dash
            log(QString("%1 takes the Dash action.").arg(actor.name));
            break;

        case 2:  // Dodge
            actor.is_dodging = true;
            log(QString("%1 takes the Dodge action (attacks against them have disadvantage until their next turn).")
                    .arg(actor.name));
            break;

        case 3:  // Drink Potion
            if (actorPotions_.empty()) {
                log(QString("%1 has no healing potions -- choose another action.").arg(actor.name));
                return;  // doesn't use up the turn's action
            }
            drinkPotion(actor);
            break;

        default: {  // Attack
            if (targetCombo_->count() == 0) return;

            int target_index = targetCombo_->currentData().toInt();
            Participant& target = participants_[static_cast<size_t>(target_index)];

            auto mode = target.is_dodging ? dnd::RollMode::Disadvantage : dnd::RollMode::Normal;
            auto result = dnd::resolve_attack(dice_, attackBonusSpin_->value(), target.armor_class,
                                               damageDiceCountSpin_->value(), damageDiceSidesSpin_->value(),
                                               damageBonusSpin_->value(), mode);
            QString outcome = applyDamage(target, result);

            log(QString("%1 attacks %2: d20(%3) %4 -> %5. %2 is now %6/%7 HP.")
                    .arg(actor.name, target.name)
                    .arg(result.attack_roll)
                    .arg(formatModifier(attackBonusSpin_->value()), outcome)
                    .arg(target.current_hp)
                    .arg(target.max_hp));
            break;
        }
    }

    refreshParticipantTable();
    checkForVictoryOrDefeat();
    // One action per turn -- Next Turn re-enables this for whoever acts next.
    if (!encounterOver_) takeActionButton_->setEnabled(false);
}

QString CombatEncounterDialog::applyDamage(Participant& target, const dnd::AttackResult& result) {
    QString outcome;
    if (!result.hit) {
        outcome = "miss";
    } else {
        target.current_hp = std::max(0, target.current_hp - result.damage_total);
        outcome = result.critical ? QString("CRITICAL HIT for %1").arg(result.damage_total)
                                   : QString("hit for %1").arg(result.damage_total);
    }

    if (target.is_character) {
        for (auto& character : characters_) {
            if (character.character_id == target.character_id) {
                character.current_hp = target.current_hp;
                repository_.update_character(character);
                break;
            }
        }
    }

    return outcome;
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
        QString victoryMessage = "Victory! All enemies defeated.";
        if (!victoryText_.isEmpty()) victoryMessage += " " + victoryText_;
        statusLabel_->setText("Status: " + victoryMessage);
        log("--- " + victoryMessage + " ---");
        awardVictoryExperience();
        awardLoot();
    }

    if (encounterOver_) takeActionButton_->setEnabled(false);
}

void CombatEncounterDialog::awardVictoryExperience() {
    int total_xp = 0;
    for (const auto& participant : participants_) {
        if (!participant.is_character) total_xp += participant.xp;
    }
    if (characters_.empty() || total_xp == 0) return;

    int share = total_xp / static_cast<int>(characters_.size());
    log(QString("The party earns %1 XP (%2 each).").arg(total_xp).arg(share));

    for (auto& character : characters_) {
        int hit_die = dnd::hit_die_for_class_id(classes_, character.class_id);
        int levels_gained = dnd::award_experience(character, share, hit_die);
        repository_.update_character(character);

        for (auto& participant : participants_) {
            if (participant.is_character && participant.character_id == character.character_id) {
                participant.current_hp = character.current_hp;
                participant.max_hp = character.max_hp;
            }
        }

        if (levels_gained > 0) {
            log(QString("*** %1 reaches level %2! Max HP is now %3. ***")
                    .arg(QString::fromStdString(character.character_name))
                    .arg(character.level)
                    .arg(character.max_hp));
        }
    }
    refreshParticipantTable();
}

void CombatEncounterDialog::awardLoot() {
    if (loot_.empty() || characters_.empty()) return;

    // First character still standing; everyone's down can't be a victory,
    // but fall back to the first character rather than dropping the loot.
    const dnd::Character* recipient = &characters_.front();
    for (const auto& character : characters_) {
        bool standing = std::any_of(participants_.begin(), participants_.end(), [&](const Participant& p) {
            return p.is_character && p.character_id == character.character_id && p.current_hp > 0;
        });
        if (standing) {
            recipient = &character;
            break;
        }
    }

    auto items = repository_.get_items();
    QStringList found;
    for (const auto& loot : loot_) {
        auto item = std::find_if(items.begin(), items.end(),
                                 [&](const dnd::Item& candidate) { return candidate.name == loot.item_name; });
        if (item == items.end()) continue;
        repository_.add_to_inventory(recipient->character_id, item->item_id, loot.quantity);
        found << QString("%1 × %2").arg(loot.quantity).arg(QString::fromStdString(item->name));
    }
    if (!found.isEmpty()) {
        log(QString("Loot! %1 takes: %2.").arg(QString::fromStdString(recipient->character_name), found.join(", ")));
    }
}

void CombatEncounterDialog::refreshPotionCombo() {
    potionCombo_->clear();
    actorPotions_.clear();
    if (currentTurnIndex_ < 0 || turnOrder_.empty()) return;

    const auto& current = participants_[static_cast<size_t>(turnOrder_[static_cast<size_t>(currentTurnIndex_)])];
    if (!current.is_character) return;

    for (const auto& entry : repository_.get_inventory(current.character_id)) {
        if (!dnd::is_healing_item(entry.item)) continue;
        actorPotions_.push_back(entry.item);
        potionCombo_->addItem(QString("%1 (×%2)").arg(QString::fromStdString(entry.item.name)).arg(entry.quantity));
    }
}

void CombatEncounterDialog::drinkPotion(Participant& actor) {
    int index = potionCombo_->currentIndex();
    if (index < 0 || index >= static_cast<int>(actorPotions_.size())) return;
    const dnd::Item item = actorPotions_[static_cast<size_t>(index)];

    for (auto& character : characters_) {
        if (character.character_id != actor.character_id) continue;

        int regained = dnd::use_healing_item(character, dice_, item);
        repository_.remove_from_inventory(character.character_id, item.item_id);
        repository_.update_character(character);
        actor.current_hp = character.current_hp;

        log(QString("%1 drinks a %2 and regains %3 HP (now %4/%5).")
                .arg(actor.name, QString::fromStdString(item.name))
                .arg(regained)
                .arg(actor.current_hp)
                .arg(actor.max_hp));
        break;
    }
    refreshPotionCombo();
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
        QString status = participant.current_hp > 0 ? "Up" : "Down";
        if (participant.current_hp > 0 && participant.is_dodging) status += " (Dodging)";
        participantTable_->setItem(row, 4, new QTableWidgetItem(status));
    }
}

void CombatEncounterDialog::refreshTargetCombo() {
    targetCombo_->clear();
    if (currentTurnIndex_ < 0 || turnOrder_.empty()) return;

    const auto& current = participants_[static_cast<size_t>(turnOrder_[static_cast<size_t>(currentTurnIndex_)])];
    for (int i = 0; i < static_cast<int>(participants_.size()); ++i) {
        const auto& candidate = participants_[static_cast<size_t>(i)];
        if (candidate.current_hp <= 0) continue;
        if (candidate.is_character == current.is_character) continue;  // only the opposing side
        targetCombo_->addItem(candidate.name, i);
    }
}

void CombatEncounterDialog::updateAttackFieldsForCurrentTurn() {
    if (currentTurnIndex_ < 0 || turnOrder_.empty()) return;

    const auto& current = participants_[static_cast<size_t>(turnOrder_[static_cast<size_t>(currentTurnIndex_)])];
    attackBonusSpin_->setValue(current.attack_bonus);
    damageDiceCountSpin_->setValue(current.damage_dice_count);
    damageDiceSidesSpin_->setValue(current.damage_dice_sides);
    damageBonusSpin_->setValue(current.damage_bonus);
}

void CombatEncounterDialog::log(const QString& line) {
    logView_->appendPlainText(line);
}
