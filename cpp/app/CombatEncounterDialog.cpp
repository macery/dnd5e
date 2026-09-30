#include "CombatEncounterDialog.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "dnd/character_creation.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

namespace {
QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}

QString ordinal(int level) {
    return QString::fromStdString(dnd::ordinal(level));
}

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

// Short ability tag for labels, e.g. "DEX".
QString abilityTag(dnd::Ability ability) {
    return QString(dnd::ability_name(ability)).left(3).toUpper();
}

enum Action { AttackAction, DashAction, DodgeAction, DrinkPotionAction, CastSpellAction, StabilizeAction };
}  // namespace

CombatEncounterDialog::CombatEncounterDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                                              std::vector<dnd::Monster> monsters, QWidget* parent,
                                              const dnd::AdventureTemplate* adventure)
    : QDialog(parent),
      repository_(repository),
      monsters_(std::move(monsters)),
      classes_(repository.get_classes()),
      dice_(randomSource_),
      encounter_(dice_, classes_, repository.get_races()) {
    setWindowTitle("Combat Encounter");

    statusLabel_ = new QLabel("Status: In Progress", this);
    statusLabel_->setStyleSheet("font-weight: bold;");

    if (adventure) {
        objectiveLabel_ = new QLabel("Objective: " + q(adventure->hook), this);
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
        "Conditions and effects in play. Hover a creature's status for what each one does and how long it lasts. "
        "\"Dying ✓○○ ✗○○\" shows death save successes and failures.");

    auto* monsterRow = new QHBoxLayout();
    monsterTypeCombo_ = new QComboBox(this);
    for (const auto& monster : monsters_) monsterTypeCombo_->addItem(q(monster.name), monster.monster_id);
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
    actionCombo_->setItemData(AttackAction, "Make one weapon attack against a target.", Qt::ToolTipRole);
    actionCombo_->addItem("Dash");
    actionCombo_->setItemData(
        DashAction, "Double your movement for the turn (informational only -- there's no map to move on).",
        Qt::ToolTipRole);
    actionCombo_->addItem("Dodge");
    actionCombo_->setItemData(DodgeAction,
        "Until the start of your next turn, attacks against you have disadvantage.", Qt::ToolTipRole);
    actionCombo_->addItem("Drink Potion");
    actionCombo_->setItemData(DrinkPotionAction, "Drink a healing potion from your inventory to regain hit points.",
                              Qt::ToolTipRole);
    actionCombo_->addItem("Cast a Spell");
    actionCombo_->setItemData(CastSpellAction,
        "Cast one of your spells. Cantrips are free; other spells use a spell slot of their level or higher.",
        Qt::ToolTipRole);
    actionCombo_->addItem("Stabilize");
    actionCombo_->setItemData(StabilizeAction,
        "Keep a dying ally from dying: a DC 10 Wisdom (Medicine) check, or automatic with a Healer's Kit. "
        "A stable creature stops making death saves.",
        Qt::ToolTipRole);
    targetCombo_ = new QComboBox(attackGroup);
    potionCombo_ = new QComboBox(attackGroup);
    stabilizeCombo_ = new QComboBox(attackGroup);
    stabilizeInfoLabel_ = new QLabel(attackGroup);
    stabilizeInfoLabel_->setWordWrap(true);
    spellCombo_ = new QComboBox(attackGroup);
    spellSlotCombo_ = new QComboBox(attackGroup);
    spellSlotCombo_->setToolTip("A higher-level slot makes many spells stronger.");
    spellTargetList_ = new QListWidget(attackGroup);
    spellTargetList_->setMaximumHeight(90);
    spellInfoLabel_ = new QLabel(attackGroup);
    spellInfoLabel_->setWordWrap(true);
    attackBonusSpin_ = new QSpinBox(attackGroup);
    attackBonusSpin_->setRange(-10, 20);
    damageDiceCountSpin_ = new QSpinBox(attackGroup);
    damageDiceCountSpin_->setRange(0, 20);  // 0: an unarmed strike's flat damage
    damageDiceSidesSpin_ = new QSpinBox(attackGroup);
    damageDiceSidesSpin_->setRange(1, 100);
    damageBonusSpin_ = new QSpinBox(attackGroup);
    damageBonusSpin_->setRange(-10, 20);
    bonusActionLabel_ = new QLabel(attackGroup);  // the turn's status: action, attacks, bonus action
    bonusActionLabel_->setWordWrap(true);
    bonusActionLabel_->setToolTip(
        "Each turn you get one action and one bonus action. Bonus-action spells (like Healing Word) use the "
        "bonus action; after casting one, the only other spell you can cast that turn is a cantrip. With Extra "
        "Attack, one Attack action is several attacks.");
    takeActionButton_ = new QPushButton("Roll Attack", attackGroup);
    takeActionButton_->setEnabled(false);
    actionSurgeButton_ = new QPushButton("Action Surge", attackGroup);
    actionSurgeButton_->setToolTip("Fighter: take one additional action this turn. Once per short rest.");
    actionSurgeButton_->setVisible(false);

    bonusCombo_ = new QComboBox(attackGroup);
    bonusTargetCombo_ = new QComboBox(attackGroup);
    bonusButton_ = new QPushButton("Use", attackGroup);
    bonusButton_->setToolTip("Use the chosen bonus action. (Bonus-action spells are under Cast a Spell.)");
    auto* bonusRow = new QWidget(attackGroup);
    auto* bonusLayout = new QHBoxLayout(bonusRow);
    bonusLayout->setContentsMargins(0, 0, 0, 0);
    bonusLayout->addWidget(bonusCombo_, 2);
    bonusLayout->addWidget(bonusTargetCombo_, 1);
    bonusLayout->addWidget(bonusButton_);
    auto* actionRow = new QWidget(attackGroup);
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->addWidget(takeActionButton_, 1);
    actionLayout->addWidget(actionSurgeButton_);

    attackForm->addRow(moveLabel_);
    attackForm->addRow("Action", actionCombo_);
    attackForm->addRow("Target", targetCombo_);
    attackForm->addRow("Potion", potionCombo_);
    attackForm->addRow("Ally", stabilizeCombo_);
    attackForm->addRow(stabilizeInfoLabel_);
    attackForm->addRow("Spell", spellCombo_);
    attackForm->addRow("Slot", spellSlotCombo_);
    attackForm->addRow("Targets", spellTargetList_);
    attackForm->addRow(spellInfoLabel_);
    attackForm->addRow("Attack Bonus", attackBonusSpin_);
    attackForm->addRow("Damage Dice Count", damageDiceCountSpin_);
    attackForm->addRow("Damage Dice Sides", damageDiceSidesSpin_);
    attackForm->addRow("Damage Bonus", damageBonusSpin_);
    attackForm->addRow(actionRow);
    attackForm->addRow("Bonus action", bonusRow);
    attackForm->addRow(bonusActionLabel_);

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
    connect(bonusButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onBonusActionClicked);
    connect(actionSurgeButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onActionSurgeClicked);
    connect(bonusCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshBonusTargets(); });
    connect(spellCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { onSpellChanged(); });
    connect(spellSlotCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshSpellInfo(); });

    for (const auto& character : characters) {
        encounter_.add_character(character, repository_.get_character_spells(character.character_id),
                                 repository_.get_inventory(character.character_id));
    }

    if (adventure) {
        for (const auto& group : adventure->monster_groups) {
            for (const auto& candidate : monsters_) {
                if (candidate.name == group.monster_name) {
                    encounter_.add_monsters(candidate, group.count);
                    break;
                }
            }
        }

        // Loot names an Item; skip any that don't match a seeded item.
        std::vector<dnd::LootGrant> loot;
        auto items = repository_.get_items();
        for (const auto& grant : adventure->loot) {
            auto item = std::find_if(items.begin(), items.end(),
                                     [&](const dnd::Item& candidate) { return candidate.name == grant.item_name; });
            if (item != items.end()) loot.push_back({*item, grant.quantity});
        }
        encounter_.set_victory_reward(adventure->victory_text, std::move(loot));
    }

    updateActionControlsVisibility();
    refreshState();
}

// --- Encounter plumbing ------------------------------------------------------

void CombatEncounterDialog::processEvents() {
    for (const auto& event : encounter_.take_events()) {
        switch (event.kind) {
            case dnd::EncounterEvent::Kind::Log:
                log(q(event.text));
                break;
            case dnd::EncounterEvent::Kind::CharacterChanged:
                if (const dnd::Character* character = encounter_.character(event.character_id)) {
                    repository_.update_character(*character);
                }
                break;
            case dnd::EncounterEvent::Kind::ItemUsed:
                repository_.remove_from_inventory(event.character_id, event.item_id, event.quantity);
                break;
            case dnd::EncounterEvent::Kind::ItemGained:
                repository_.add_to_inventory(event.character_id, event.item_id, event.quantity);
                break;
        }
    }
}

void CombatEncounterDialog::onTurnStarted() {
    const dnd::Combatant* current = encounter_.current();
    if (current && encounter_.awaiting_action()) {
        currentTurnLabel_->setText("Current Turn: " + q(current->name));
        actionCombo_->setCurrentIndex(AttackAction);
        refreshTargetCombo();
        refreshPotionCombo();
        refreshStabilizeCombo();
        refreshSpellControls();
        updateAttackFieldsForCurrentTurn();
        updateActionControlsVisibility();
    } else if (encounter_.status() != dnd::EncounterStatus::InProgress) {
        currentTurnLabel_->setText("Current Turn: -");
        targetCombo_->clear();
    }
}

void CombatEncounterDialog::refreshState() {
    if (encounter_.status() != dnd::EncounterStatus::InProgress) {
        statusLabel_->setText("Status: " + q(encounter_.result_message()));
    }
    refreshParticipantTable();
    takeActionButton_->setEnabled(encounter_.awaiting_action() || encounter_.bonus_action_available());
    refreshBonusControls();

    // "Action: available · Bonus action: used", plus attacks left in an Attack action.
    if (!encounter_.awaiting_action() && !encounter_.bonus_action_available()) {
        bonusActionLabel_->setText(encounter_.current() ? "Nothing left this turn: click Next Turn." : "");
        return;
    }
    const auto& turn = encounter_.turn();
    QStringList parts;
    if (turn.attacks_left > 0) {
        parts << QString("Attacks left: %1 (Extra Attack)").arg(turn.attacks_left);
    } else {
        parts << (turn.actions_left > 1   ? QString("Actions: %1").arg(turn.actions_left)
                  : turn.actions_left == 1 ? QString("Action: available")
                                           : QString("Action: used"));
    }
    parts << (turn.bonus_action_used ? "Bonus action: used" : "Bonus action: available");
    bonusActionLabel_->setText(parts.join(" · "));
}

// --- Buttons -----------------------------------------------------------------

void CombatEncounterDialog::onAddMonsterClicked() {
    if (monsterTypeCombo_->count() == 0) return;

    int monster_id = monsterTypeCombo_->currentData().toInt();
    auto monster = std::find_if(monsters_.begin(), monsters_.end(),
                                [monster_id](const dnd::Monster& m) { return m.monster_id == monster_id; });
    if (monster == monsters_.end()) return;

    int count = monsterCountSpin_->value();
    encounter_.add_monsters(*monster, count);
    log(QString("%1 x%2 joins the encounter.").arg(q(monster->name)).arg(count) +
        (encounter_.started() ? " (click Roll Initiative to include them in turn order)" : ""));
    refreshState();
}

void CombatEncounterDialog::onRollInitiativeClicked() {
    encounter_.roll_initiative();
    processEvents();
    onTurnStarted();
    refreshState();
}

void CombatEncounterDialog::onNextTurnClicked() {
    encounter_.next_turn();
    processEvents();
    onTurnStarted();
    refreshState();
}

void CombatEncounterDialog::onActionChanged(int /*index*/) {
    updateActionControlsVisibility();
    refreshSpellInfo();
}

void CombatEncounterDialog::onTakeActionClicked() {
    bool acted = false;
    switch (actionCombo_->currentIndex()) {
        case DashAction:
            acted = encounter_.dash();
            break;
        case DodgeAction:
            acted = encounter_.dodge();
            break;
        case DrinkPotionAction:
            acted = encounter_.drink_potion(potionCombo_->currentData().toInt());
            break;
        case StabilizeAction:
            if (stabilizeCombo_->count() == 0) {
                log("No one is dying -- choose another action.");
                return;
            }
            acted = encounter_.stabilize(stabilizeCombo_->currentData().toInt());
            break;
        case CastSpellAction: {
            const dnd::Spell* spell = selectedSpell();
            std::vector<int> targets;
            for (int row = 0; row < spellTargetList_->count(); ++row) {
                if (spellTargetList_->item(row)->checkState() == Qt::Checked) {
                    targets.push_back(spellTargetList_->item(row)->data(Qt::UserRole).toInt());
                }
            }
            int slot = spell && spell->level > 0 ? spellSlotCombo_->currentData().toInt() : 0;
            acted = encounter_.cast_spell(spell ? spell->spell_id : 0, slot, targets);
            break;
        }
        default:
            if (targetCombo_->count() == 0) return;
            acted = encounter_.attack(targetCombo_->currentData().toInt(), attackBonusSpin_->value(),
                                      damageDiceCountSpin_->value(), damageDiceSidesSpin_->value(),
                                      damageBonusSpin_->value());
            break;
    }

    processEvents();
    if (acted) afterCharacterActed();
    if (encounter_.status() != dnd::EncounterStatus::InProgress) onTurnStarted();
    refreshState();
}

void CombatEncounterDialog::afterCharacterActed() {
    refreshTargetCombo();
    refreshPotionCombo();
    refreshStabilizeCombo();
    refreshSpellControls();
    updateActionControlsVisibility();
}

void CombatEncounterDialog::onBonusActionClicked() {
    int index = bonusCombo_->currentIndex();
    if (index < 0 || index >= static_cast<int>(bonusTargets_.size())) return;
    auto kind = static_cast<dnd::Encounter::BonusAction>(bonusCombo_->currentData().toInt());
    int target = bonusTargetCombo_->isVisible() ? bonusTargetCombo_->currentData().toInt() : -1;
    bool used = encounter_.use_bonus_action(kind, target);
    processEvents();
    if (used) afterCharacterActed();
    if (encounter_.status() != dnd::EncounterStatus::InProgress) onTurnStarted();
    refreshState();
}

void CombatEncounterDialog::onActionSurgeClicked() {
    encounter_.action_surge();
    processEvents();
    afterCharacterActed();
    refreshState();
}

void CombatEncounterDialog::refreshBonusControls() {
    bonusCombo_->blockSignals(true);
    bonusCombo_->clear();
    bonusTargets_.clear();
    for (const auto& option : encounter_.bonus_action_options()) {
        bonusCombo_->addItem(q(option.label), static_cast<int>(option.kind));
        bonusCombo_->setItemData(bonusCombo_->count() - 1, q(option.description), Qt::ToolTipRole);
        bonusTargets_.push_back(option.target);
    }
    bool any = bonusCombo_->count() > 0;
    if (!any) bonusCombo_->addItem(encounter_.bonus_action_available() ? "(none available)" : "(used)");
    bonusCombo_->setEnabled(any);
    bonusButton_->setEnabled(any);
    bonusCombo_->blockSignals(false);
    refreshBonusTargets();
    actionSurgeButton_->setVisible(encounter_.can_action_surge());
}

void CombatEncounterDialog::refreshBonusTargets() {
    bonusTargetCombo_->clear();
    int index = bonusCombo_->currentIndex();
    bool needsTarget = index >= 0 && index < static_cast<int>(bonusTargets_.size()) &&
                       bonusTargets_[static_cast<size_t>(index)] != dnd::Encounter::BonusTarget::None;
    bonusTargetCombo_->setVisible(needsTarget);
    if (!needsTarget) return;
    for (int target : encounter_.bonus_action_targets(bonusTargets_[static_cast<size_t>(index)])) {
        bonusTargetCombo_->addItem(q(encounter_.combatants()[static_cast<size_t>(target)].name), target);
    }
}

// --- Views -------------------------------------------------------------------

void CombatEncounterDialog::updateActionControlsVisibility() {
    bool is_attack = actionCombo_->currentIndex() == AttackAction;
    for (auto* field : {static_cast<QWidget*>(targetCombo_), static_cast<QWidget*>(attackBonusSpin_),
                         static_cast<QWidget*>(damageDiceCountSpin_), static_cast<QWidget*>(damageDiceSidesSpin_),
                         static_cast<QWidget*>(damageBonusSpin_)}) {
        field->setVisible(is_attack);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(is_attack);
    }
    bool is_potion = actionCombo_->currentIndex() == DrinkPotionAction;
    potionCombo_->setVisible(is_potion);
    if (auto* row_label = attackFormLayout_->labelForField(potionCombo_)) row_label->setVisible(is_potion);

    bool is_stabilize = actionCombo_->currentIndex() == StabilizeAction;
    for (auto* field : {static_cast<QWidget*>(stabilizeCombo_), static_cast<QWidget*>(stabilizeInfoLabel_)}) {
        field->setVisible(is_stabilize);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(is_stabilize);
    }

    bool is_spell = actionCombo_->currentIndex() == CastSpellAction;
    const dnd::Spell* spell = is_spell ? selectedSpell() : nullptr;
    bool show_slot = spell && spell->level > 0;
    bool show_targets = spell && !dnd::spell_targets_nobody(*spell);
    for (auto [field, visible] : {std::pair<QWidget*, bool>{spellCombo_, is_spell}, {spellSlotCombo_, show_slot},
                                  {spellTargetList_, show_targets}, {spellInfoLabel_, is_spell}}) {
        field->setVisible(visible);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(visible);
    }

    switch (actionCombo_->currentIndex()) {
        case DashAction: takeActionButton_->setText("Take Dash Action"); break;
        case DodgeAction: takeActionButton_->setText("Take Dodge Action"); break;
        case DrinkPotionAction: takeActionButton_->setText("Drink Potion"); break;
        case CastSpellAction:
            takeActionButton_->setText(spell && spell->casting_time == dnd::CastingTime::BonusAction
                                           ? "Cast Spell (bonus action)"
                                           : "Cast Spell");
            break;
        case StabilizeAction: takeActionButton_->setText("Stabilize"); break;
        default: {
            const dnd::Combatant* current = encounter_.current();
            takeActionButton_->setText(current && !current->attack_name.empty()
                                           ? QString("Roll Attack (%1)").arg(q(current->attack_name))
                                           : QString("Roll Attack"));
            break;
        }
    }
}

void CombatEncounterDialog::refreshParticipantTable() {
    const auto& combatants = encounter_.combatants();
    std::vector<size_t> order(combatants.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&combatants](size_t a, size_t b) { return combatants[a].initiative > combatants[b].initiative; });

    participantTable_->setRowCount(static_cast<int>(order.size()));
    for (int row = 0; row < static_cast<int>(order.size()); ++row) {
        const auto& combatant = combatants[order[static_cast<size_t>(row)]];
        participantTable_->setItem(row, 0, new QTableWidgetItem(q(combatant.name)));
        participantTable_->setItem(row, 1, new QTableWidgetItem(QString::number(combatant.initiative)));
        participantTable_->setItem(row, 2, new QTableWidgetItem(QString::number(combatant.armor_class)));
        participantTable_->setItem(
            row, 3, new QTableWidgetItem(QString("%1/%2").arg(combatant.current_hp).arg(combatant.max_hp)));
        int index = static_cast<int>(order[static_cast<size_t>(row)]);
        auto* status = new QTableWidgetItem(q(encounter_.status_text(index)));
        QStringList tooltip;
        for (const auto& line : encounter_.status_tooltip(index)) tooltip << q(line).toHtmlEscaped();
        status->setToolTip(tooltip.join("<br>"));
        if (combatant.death_saves.dead || (combatant.is_character && combatant.current_hp <= 0 &&
                                           !combatant.death_saves.stable)) {
            status->setForeground(QBrush(QColor(176, 0, 32)));  // dying or dead
        } else if (combatant.current_hp <= 0) {
            status->setForeground(QBrush(QColor(128, 128, 128)));  // stable, or a defeated monster
        }
        participantTable_->setItem(row, 4, status);
    }
}

void CombatEncounterDialog::refreshTargetCombo() {
    targetCombo_->clear();
    for (int index : encounter_.attack_targets()) {
        targetCombo_->addItem(q(encounter_.combatants()[static_cast<size_t>(index)].name), index);
    }
}

void CombatEncounterDialog::refreshPotionCombo() {
    potionCombo_->clear();
    for (const auto& entry : encounter_.healing_items()) {
        potionCombo_->addItem(QString("%1 (×%2)").arg(q(entry.item.name)).arg(entry.quantity), entry.item.item_id);
    }
}

void CombatEncounterDialog::refreshStabilizeCombo() {
    stabilizeCombo_->clear();
    for (int index : encounter_.stabilize_targets()) {
        stabilizeCombo_->addItem(q(encounter_.combatants()[static_cast<size_t>(index)].name), index);
    }
    if (stabilizeCombo_->count() == 0) {
        stabilizeInfoLabel_->setText("No one is dying.");
    } else if (encounter_.has_healers_kit()) {
        stabilizeInfoLabel_->setText("Automatic: you have a Healer's Kit.");
    } else {
        stabilizeInfoLabel_->setText("DC 10 Wisdom (Medicine) check.");
    }
}

void CombatEncounterDialog::updateAttackFieldsForCurrentTurn() {
    const dnd::Combatant* current = encounter_.current();
    if (!current) return;
    attackBonusSpin_->setValue(current->attack_bonus);
    damageDiceCountSpin_->setValue(current->damage_dice_count);
    damageDiceSidesSpin_->setValue(current->damage_dice_sides);
    damageBonusSpin_->setValue(current->damage_bonus);
}

// --- Spells ------------------------------------------------------------------

const dnd::Spell* CombatEncounterDialog::selectedSpell() const {
    int index = spellCombo_->currentIndex();
    if (index < 0 || index >= static_cast<int>(castableSpells_.size())) return nullptr;
    return &castableSpells_[static_cast<size_t>(index)];
}

void CombatEncounterDialog::refreshSpellControls() {
    castableSpells_ = encounter_.castable_spells();
    spellCombo_->blockSignals(true);
    spellCombo_->clear();
    for (const auto& spell : castableSpells_) {
        QString level = spell.level == 0 ? "cantrip" : ordinal(spell.level);
        if (spell.casting_time == dnd::CastingTime::BonusAction) level += ", bonus action";
        spellCombo_->addItem(QString("%1 (%2)").arg(q(spell.name), level), spell.spell_id);
        spellCombo_->setItemData(spellCombo_->count() - 1, q(spell.description), Qt::ToolTipRole);
    }
    spellCombo_->blockSignals(false);
    onSpellChanged();
}

void CombatEncounterDialog::onSpellChanged() {
    spellSlotCombo_->blockSignals(true);
    spellSlotCombo_->clear();
    spellTargetList_->clear();

    if (const dnd::Spell* spell = selectedSpell()) {
        const dnd::Combatant* current = encounter_.current();
        const dnd::Character* character = current ? encounter_.character(current->character_id) : nullptr;
        const dnd::ClassInfo* info = character ? dnd::find_class_info_by_id(classes_, character->class_id) : nullptr;
        for (int level : encounter_.spell_slot_options(*spell)) {
            int remaining = info ? dnd::slots_remaining(*character, *info, level) : 0;
            spellSlotCombo_->addItem(QString("%1 level (%2 left)").arg(ordinal(level)).arg(remaining), level);
        }

        for (int index : encounter_.spell_target_candidates(*spell)) {
            const auto& candidate = encounter_.combatants()[static_cast<size_t>(index)];
            auto* item = new QListWidgetItem(
                QString("%1 (%2/%3 HP)").arg(q(candidate.name)).arg(candidate.current_hp).arg(candidate.max_hp),
                spellTargetList_);
            item->setData(Qt::UserRole, index);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(spellTargetList_->count() == 1 ? Qt::Checked : Qt::Unchecked);
        }
    }
    spellSlotCombo_->blockSignals(false);
    refreshSpellInfo();
    updateActionControlsVisibility();
}

void CombatEncounterDialog::refreshSpellInfo() {
    const dnd::Spell* spell = selectedSpell();
    if (!spell) {
        const dnd::Combatant* current = encounter_.current();
        QString why = current && current->armor_not_proficient
            ? "Can't cast spells while wearing armor you aren't proficient with (take it off in the Inventory)."
            : "No spells to cast (none known, or no slots left).";
        spellInfoLabel_->setText(spellCombo_->count() == 0 && actionCombo_->currentIndex() == CastSpellAction
                                     ? why
                                     : "");
        return;
    }
    const dnd::Combatant* current = encounter_.current();
    const dnd::Character* character = current ? encounter_.character(current->character_id) : nullptr;
    const dnd::ClassInfo* info = character ? dnd::find_class_info_by_id(classes_, character->class_id) : nullptr;
    if (!character || !info) return;

    auto caster = dnd::caster_stats(*character, *info);
    int slot = spell->level == 0 ? 0 : spellSlotCombo_->currentData().toInt();
    int dice = dnd::spell_dice_count(*spell, caster.level, slot);
    int projectiles = dnd::spell_projectiles(*spell, caster.level, slot);
    QString roll = QString("%1d%2").arg(dice).arg(spell->dice_sides);
    int bonus = spell->flat_bonus + (spell->adds_modifier ? caster.spellcasting_modifier : 0);
    if (bonus) roll += formatModifier(bonus);
    if (projectiles > 1) roll = QString("%1 × %2").arg(projectiles).arg(roll);

    QString text;
    switch (spell->resolution) {
        case dnd::SpellResolution::Attack:
            text = QString("%1 spell attack %2, %3 %4 damage.")
                       .arg(spell->melee_attack ? "Melee" : "Ranged", formatModifier(caster.spell_attack_bonus), roll,
                            q(spell->damage_type));
            break;
        case dnd::SpellResolution::Save:
            text = spell->dice_count > 0
                ? QString("Each target makes a %1 save vs DC %2: %3 %4 damage, %5 on a success.")
                      .arg(abilityTag(spell->save_ability))
                      .arg(caster.spell_save_dc)
                      .arg(roll, q(spell->damage_type), spell->save_halves ? "half" : "none")
                : QString("Each target makes a %1 save vs DC %2%3.")
                      .arg(abilityTag(spell->save_ability))
                      .arg(caster.spell_save_dc)
                      .arg(spell->combat_save_advantage ? " (with advantage, since you're fighting it)" : "");
            break;
        case dnd::SpellResolution::AutoHit:
            text = QString("Always hits: %1 %2 damage.").arg(roll, q(spell->damage_type));
            break;
        case dnd::SpellResolution::Heal:
            text = QString("Heals %1.").arg(roll);
            break;
        case dnd::SpellResolution::Utility:
            text = "No automated effect yet -- casting uses the action (and slot) and shows the description.";
            break;
        case dnd::SpellResolution::Buff:
        case dnd::SpellResolution::Stabilize: {
            // "buff: Blessed (up to 3 allies) (concentration)" -> "Buff: Blessed (up to 3 allies)."
            QString summary = q(dnd::spell_summary(*spell)).remove(" (concentration)");
            text = summary.left(1).toUpper() + summary.mid(1) + ".";
            break;
        }
    }

    // Riders from the spell's effects, e.g. "On a hit: Guiding Bolt."
    QStringList onHit;
    QStringList onFail;
    for (const auto& effect : spell->effects) {
        QString label = q(effect.label);
        if (effect.trigger == dnd::SpellEffectTrigger::OnHit && !onHit.contains(label)) onHit << label;
        if (effect.trigger == dnd::SpellEffectTrigger::OnFailedSave && !onFail.contains(label)) onFail << label;
    }
    if (!onHit.isEmpty()) text += " On a hit: " + onHit.join(", ") + ".";
    if (!onFail.isEmpty()) text += " On a failure: " + onFail.join(", ") + ".";
    if (spell->concentration) text += " Requires concentration.";
    if (!dnd::spell_targets_nobody(*spell)) {
        int max_targets = dnd::spell_max_targets(*spell, caster.level, slot);
        text += max_targets > 1 ? QString(" Choose up to %1 targets.").arg(max_targets) : " Choose 1 target.";
    }
    spellInfoLabel_->setText(text);
}

void CombatEncounterDialog::log(const QString& line) {
    logView_->appendPlainText(line);
}
