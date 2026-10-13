#include "CombatEncounterDialog.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

#include "WindowSizing.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellbook.hpp"
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

// LayOnHandsAction is last: it's only in the combo on a Paladin's turn.
enum Action { AttackAction, DashAction, DodgeAction, DrinkPotionAction, CastSpellAction, StabilizeAction,
              ReadScrollAction, HelpAction, LayOnHandsAction };
}  // namespace

CombatEncounterDialog::CombatEncounterDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                                              std::vector<dnd::Monster> monsters, QWidget* parent,
                                              const AdventureFight* fight)
    : QDialog(parent),
      repository_(repository),
      monsters_(std::move(monsters)),
      classes_(repository.get_classes()),
      dice_(randomSource_),
      encounter_(dice_, classes_, repository.get_races()) {
    setWindowTitle(fight ? fight->title : QString("Combat Encounter"));

    statusLabel_ = new QLabel("Status: In Progress", this);
    statusLabel_->setStyleSheet("font-weight: bold;");

    if (fight) {
        QString objective = QString("<b>%1.</b> %2").arg(q(fight->scene.title).toHtmlEscaped(),
                                                         q(fight->scene.narration).toHtmlEscaped());
        QStringList allies;
        for (const auto& ally : fight->allies) allies << q(ally.npc.name).toHtmlEscaped();
        if (!allies.isEmpty()) objective += QString("<br><i>Fighting beside you: %1.</i>").arg(allies.join(", "));
        QStringList foes;
        for (const auto& foe : fight->foes) foes << q(foe.npc.name).toHtmlEscaped();
        if (!foes.isEmpty()) objective += QString("<br><i>Against you: %1.</i>").arg(foes.join(", "));
        objectiveLabel_ = new QLabel(objective, this);
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
    difficultyLabel_ = new QLabel(this);
    difficultyLabel_->setWordWrap(true);
    difficultyLabel_->setToolTip(
        "How hard this fight is for the characters in it, from the 2014 encounter-building rules: the monsters' "
        "XP, multiplied for how many there are (more monsters are more dangerous), against the party's Easy / "
        "Medium / Hard / Deadly thresholds. Deadly fights can kill characters. The XP awarded on victory is the "
        "unmultiplied total.");

    auto* turnRow = new QHBoxLayout();
    rollInitiativeButton_ = new QPushButton("Roll Initiative", this);
    nextTurnButton_ = new QPushButton("Next Turn", this);
    currentTurnLabel_ = new QLabel("Current Turn: -", this);
    currentTurnLabel_->setStyleSheet("font-weight: bold;");
    turnRow->addWidget(rollInitiativeButton_);
    turnRow->addWidget(nextTurnButton_);
    continueButton_ = new QPushButton("Continue", this);
    continueButton_->setToolTip("The fight is over: back to the adventure.");
    continueButton_->setVisible(false);
    turnRow->addWidget(continueButton_);
    auto* reactionsBox = new QCheckBox("Automatic reactions", this);
    reactionsBox->setChecked(true);
    reactionsBox->setToolTip(
        "Reactions happen on monsters' turns, so they're taken for you when they help: Shield (Sorcerers, "
        "Wizards) when +5 AC turns a hit into a miss, a level-5 Rogue's Uncanny Dodge to halve a hit, and Hellish "
        "Rebuke (Warlocks) when hurt. Each spends a slot (Uncanny Dodge spends nothing) and the character's reaction "
        "until their next turn. Uncheck to save slots.");
    connect(reactionsBox, &QCheckBox::toggled, this, [this](bool on) { encounter_.set_auto_reactions(on); });
    reactionsBox_ = reactionsBox;  // on its own line: the turn row is already as wide as small screens allow
    turnRow->addStretch(1);
    turnRow->addWidget(currentTurnLabel_);

    auto* attackGroup = new QGroupBox("Your Turn", this);
    auto* attackForm = new GridForm(attackGroup);
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
    actionCombo_->addItem("Read Scroll");
    actionCombo_->setItemData(ReadScrollAction,
        "Cast the spell on a spell scroll from your pack: no slot needed, and the scroll is used up. Only classes "
        "with the spell on their list can read it; a spell above your highest slot level needs a spellcasting check "
        "(DC 10 + its level). The scroll's own save DC and attack bonus apply.",
        Qt::ToolTipRole);
    actionCombo_->addItem("Help");
    actionCombo_->setItemData(HelpAction,
        "Give an ally advantage on their next attack roll, until the start of your next turn. (In the PHB the "
        "enemy must be within 5 feet of you; without a map, any ally can be helped.)",
        Qt::ToolTipRole);
    targetCombo_ = new QComboBox(attackGroup);
    helpCombo_ = new QComboBox(attackGroup);
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
    recklessBox_ = new QCheckBox("Attack recklessly", attackGroup);
    recklessBox_->setToolTip(
        "Barbarian Reckless Attack, chosen with your first attack of the turn: advantage on STR melee weapon "
        "attacks this turn, but attacks against you have advantage until your next turn.");
    smiteCombo_ = new QComboBox(attackGroup);
    smiteCombo_->setToolTip(
        "Paladin Divine Smite: if this attack hits, spend a spell slot for extra radiant damage -- 2d8 with a "
        "1st-level slot, +1d8 per level above 1st (max 5d8). A miss spends nothing.");
    layOnHandsTargetCombo_ = new QComboBox(attackGroup);
    layOnHandsSpin_ = new QSpinBox(attackGroup);
    layOnHandsSpin_->setToolTip("Hit points to spend from your Lay on Hands pool.");
    layOnHandsInfoLabel_ = new QLabel(attackGroup);
    layOnHandsInfoLabel_->setWordWrap(true);
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
    attackForm->addRow("Help", helpCombo_);
    attackForm->addRow(stabilizeInfoLabel_);
    attackForm->addRow("Spell", spellCombo_);
    attackForm->addRow("Slot", spellSlotCombo_);
    metamagicCombo_ = new QComboBox(attackGroup);
    metamagicCombo_->setToolTip("Sorcerer Metamagic: shape the spell with sorcery points. Quickened casts it as a "
                                "bonus action, Twinned adds a second target, Empowered rerolls low damage dice, "
                                "Heightened gives one target disadvantage on its save.");
    attackForm->addRow("Metamagic", metamagicCombo_);
    attackForm->addRow("Targets", spellTargetList_);
    attackForm->addRow(spellInfoLabel_);
    attackForm->addRow("Attack Bonus", attackBonusSpin_);
    attackForm->addRow("Damage Dice Count", damageDiceCountSpin_);
    attackForm->addRow("Damage Dice Sides", damageDiceSidesSpin_);
    attackForm->addRow("Damage Bonus", damageBonusSpin_);
    attackForm->addRow("Reckless", recklessBox_);
    stunningBox_ = new QCheckBox("Stunning Strike (1 ki)", attackGroup);
    stunningBox_->setToolTip("On a hit, spend 1 ki: the target makes a CON save or is stunned until the end of "
                             "your next turn. No ki is spent on a miss.");
    attackForm->addRow("Stunning Strike", stunningBox_);
    inspirationBox_ = new QCheckBox("Use inspiration (advantage)", attackGroup);
    inspirationBox_->setToolTip("Inspiration (earned in adventure scenes): advantage on this attack. Spent once used.");
    attackForm->addRow("Inspiration", inspirationBox_);
    attackForm->addRow("Divine Smite", smiteCombo_);
    attackForm->addRow("Touch", layOnHandsTargetCombo_);
    attackForm->addRow("Hit points", layOnHandsSpin_);
    attackForm->addRow(layOnHandsInfoLabel_);
    attackForm->addRow(actionRow);
    attackForm->addRow("Bonus action", bonusRow);
    channelCombo_ = new QComboBox(attackGroup);
    channelButton_ = new QPushButton("Channel", attackGroup);
    channelButton_->setToolTip("Channel Divinity takes your action and one use (back on a short rest).");
    channelRow_ = new QWidget(attackGroup);
    auto* channelLayout = new QHBoxLayout(channelRow_);
    channelLayout->setContentsMargins(0, 0, 0, 0);
    channelLayout->addWidget(channelCombo_, 1);
    channelLayout->addWidget(channelButton_);
    attackForm->addRow("Channel Divinity", channelRow_);
    connect(channelButton_, &QPushButton::clicked, this, &CombatEncounterDialog::onChannelClicked);
    wildShapeCombo_ = new QComboBox(attackGroup);
    auto* wildShapeButton = new QPushButton("Transform", attackGroup);
    wildShapeButton->setToolTip("Wild Shape takes your action and a use (two per short rest). The beast's HP, AC, and "
                                "attacks replace yours; at 0 HP you change back and extra damage carries over. A bonus "
                                "action changes you back sooner. No spellcasting in beast form.");
    wildShapeRow_ = new QWidget(attackGroup);
    auto* wildShapeLayout = new QHBoxLayout(wildShapeRow_);
    wildShapeLayout->setContentsMargins(0, 0, 0, 0);
    wildShapeLayout->addWidget(wildShapeCombo_, 1);
    wildShapeLayout->addWidget(wildShapeButton);
    attackForm->addRow("Wild Shape", wildShapeRow_);
    connect(wildShapeButton, &QPushButton::clicked, this, &CombatEncounterDialog::onWildShapeClicked);
    attackForm->addRow(bonusActionLabel_);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);

    // Layout: the fight (status, setup, participants, log) and "Your Turn"
    // side by side in a splitter -- stacked when the window is narrow (see
    // resizeEvent). The participants and log share a splitter of their own,
    // and the turn box scrolls when it doesn't fit.
    auto* logPanel = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->addWidget(new QLabel("Log", logPanel));
    logLayout->addWidget(logView_, 1);
    auto* tableAndLog = new QSplitter(Qt::Vertical, this);
    tableAndLog->addWidget(participantTable_);
    tableAndLog->addWidget(logPanel);
    tableAndLog->setStretchFactor(0, 2);
    tableAndLog->setStretchFactor(1, 3);
    tableAndLog->setChildrenCollapsible(false);

    auto* fightPanel = new QWidget(this);
    auto* fightLayout = new QVBoxLayout(fightPanel);
    fightLayout->setContentsMargins(0, 0, 0, 0);
    fightLayout->addWidget(statusLabel_);
    if (objectiveLabel_) fightLayout->addWidget(objectiveLabel_);
    fightLayout->addLayout(monsterRow);
    fightLayout->addWidget(difficultyLabel_);
    fightLayout->addLayout(turnRow);
    fightLayout->addWidget(reactionsBox_);
    fightLayout->addWidget(tableAndLog, 1);

    auto* turnPanel = new QWidget(this);
    auto* turnLayout = new QVBoxLayout(turnPanel);
    turnLayout->setContentsMargins(0, 0, 0, 0);
    lootGroup_ = new QGroupBox("Divide the Loot", this);
    lootGroup_->setToolTip("Choose who takes each item. Healing potions start with whoever is most hurt, and the "
                           "rest is spread around. Closing the window hands it out as shown.");
    auto* lootLayout = new QVBoxLayout(lootGroup_);
    lootGrid_ = new QGridLayout();
    lootLayout->addLayout(lootGrid_);
    auto* handOutButton = new QPushButton("Hand Out Loot", lootGroup_);
    auto* handOutRow = new QHBoxLayout();
    handOutRow->addStretch(1);
    handOutRow->addWidget(handOutButton);
    lootLayout->addLayout(handOutRow);
    lootGroup_->setVisible(false);
    connect(handOutButton, &QPushButton::clicked, this, &CombatEncounterDialog::handOutLoot);
    turnLayout->addWidget(lootGroup_);
    turnLayout->addWidget(attackGroup);
    turnLayout->addStretch(1);

    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    mainSplitter_->addWidget(fightPanel);
    mainSplitter_->addWidget(scrollable(turnPanel, this));
    mainSplitter_->setStretchFactor(0, 3);
    mainSplitter_->setStretchFactor(1, 2);
    mainSplitter_->setChildrenCollapsible(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(mainSplitter_);
    fitToScreen(this, QSize(1000, 720), "CombatEncounter");

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
    connect(layOnHandsTargetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshLayOnHandsAmount(); });
    connect(spellSlotCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                refreshSpellInfo();
                refreshMetamagic();
            });

    encounter_.set_scroll_spells(repository_.get_spells());  // what scrolls in packs can cast
    encounter_.set_wild_shape_beasts(monsters_);             // what druids can become
    for (const auto& character : characters) {
        encounter_.add_character(character, dnd::spells_ready(repository_, character),
                                 repository_.get_inventory(character.character_id));
    }

    if (fight) {
        connect(continueButton_, &QPushButton::clicked, this, &QDialog::accept);
        for (const auto& group : fight->scene.monsters) {
            for (const auto& candidate : monsters_) {
                if (candidate.name == group.monster_name) {
                    encounter_.add_monsters(candidate, group.count);
                    break;
                }
            }
        }

        for (const auto& ally : fight->allies) {
            encounter_.add_ally(ally.stat_block, ally.npc.name + " (" + ally.stat_block.name + ")", ally.npc.npc_id);
        }
        for (const auto& foe : fight->foes) {
            encounter_.add_named_foe(foe.stat_block, foe.npc.name + " (" + foe.stat_block.name + ")", foe.npc.npc_id);
        }

        // Loot names an Item; skip any that don't match a seeded item.
        std::vector<dnd::LootGrant> loot;
        auto items = repository_.get_items();
        for (const auto& grant : fight->scene.loot) {
            auto item = std::find_if(items.begin(), items.end(),
                                     [&](const dnd::Item& candidate) { return candidate.name == grant.item_name; });
            if (item != items.end()) loot.push_back({*item, grant.quantity});
        }
        encounter_.set_victory_reward(fight->victory_text, std::move(loot), fight->scene.coins_cp);
        encounter_.set_surprise(fight->surprise);
    }

    updateActionControlsVisibility();
    refreshState();
}

void CombatEncounterDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    // Side by side when there's room; the turn box below the fight otherwise.
    Qt::Orientation wanted = event->size().width() >= 820 ? Qt::Horizontal : Qt::Vertical;
    if (mainSplitter_ && mainSplitter_->orientation() != wanted) {
        mainSplitter_->setOrientation(wanted);
        // Rebalance for the new direction: the fight gets a bit more than half.
        int total = wanted == Qt::Horizontal ? event->size().width() : event->size().height();
        mainSplitter_->setSizes({total * 55 / 100, total * 45 / 100});
    }
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

// --- Loot --------------------------------------------------------------------

void CombatEncounterDialog::refreshLootPanel() {
    if (!encounter_.loot_pending()) {
        lootGroup_->setVisible(false);
        return;
    }
    if (!lootRows_.empty()) return;  // already built for this loot

    auto recipients = encounter_.loot_recipients();
    auto suggested = encounter_.suggested_loot_shares();
    // Small stacks get a row per item so they can be split; big ones (a
    // bundle of arrows) one row per suggested share.
    std::map<int, int> totals;
    for (const auto& grant : encounter_.pending_loot()) totals[grant.item.item_id] += grant.quantity;
    for (const auto& share : suggested) {
        int rows = totals[share.item.item_id] <= 10 ? share.quantity : 1;
        int each = share.quantity / rows;
        for (int i = 0; i < rows; ++i) {
            int row = static_cast<int>(lootRows_.size());
            auto* label = new QLabel(QString("%1 × %2").arg(each).arg(q(share.item.name)), lootGroup_);
            label->setToolTip(q(share.item.description));
            auto* combo = new QComboBox(lootGroup_);
            for (const auto& recipient : recipients) {
                const dnd::Character* character = encounter_.character(recipient.character_id);
                combo->addItem(QString("%1 (%2/%3 HP)")
                                   .arg(character ? q(character->character_name) : QString("?"))
                                   .arg(recipient.current_hp)
                                   .arg(recipient.max_hp),
                               recipient.character_id);
            }
            combo->setCurrentIndex(std::max(0, combo->findData(share.character_id)));
            lootGrid_->addWidget(label, row, 0);
            lootGrid_->addWidget(combo, row, 1);
            lootRows_.push_back({share.item, each, combo});
        }
    }
    lootGrid_->setColumnStretch(1, 1);
    lootGroup_->setVisible(true);
}

void CombatEncounterDialog::handOutLoot() {
    if (!encounter_.loot_pending()) return;
    std::vector<dnd::LootShare> shares;
    for (const auto& row : lootRows_) {
        int who = row.recipient->currentData().toInt();
        auto same = std::find_if(shares.begin(), shares.end(), [&](const dnd::LootShare& s) {
            return s.item.item_id == row.item.item_id && s.character_id == who;
        });
        if (same != shares.end()) {
            same->quantity += row.quantity;
        } else {
            shares.push_back({row.item, row.quantity, who});
        }
    }
    if (!encounter_.divide_loot(shares)) encounter_.divide_loot(encounter_.suggested_loot_shares());
    processEvents();
    refreshState();
}

void CombatEncounterDialog::done(int result) {
    handOutLoot();  // never lose the loot by closing the window
    QDialog::done(result);
}

void CombatEncounterDialog::onTurnStarted() {
    const dnd::Combatant* current = encounter_.current();
    if (current && encounter_.awaiting_action()) {
        currentTurnLabel_->setText("Current Turn: " + q(current->name));
        actionCombo_->setCurrentIndex(AttackAction);
        // Lay on Hands is only offered on a Paladin's turn.
        bool paladin = current->class_name == "Paladin";
        if (paladin && actionCombo_->count() == LayOnHandsAction) {
            actionCombo_->addItem("Lay on Hands");
            actionCombo_->setItemData(LayOnHandsAction,
                "Touch a creature and restore hit points from your Lay on Hands pool (5 × paladin level, back on a "
                "long rest).",
                Qt::ToolTipRole);
        } else if (!paladin && actionCombo_->count() > LayOnHandsAction) {
            actionCombo_->removeItem(LayOnHandsAction);
        }
        refreshTargetCombo();
        refreshPotionCombo();
        refreshStabilizeCombo();
        refreshHelpCombo();
        refreshSpellControls();
        refreshFeatureControls();
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
        if (objectiveLabel_) {  // an adventure fight: on to what comes next
            continueButton_->setText(encounter_.status() == dnd::EncounterStatus::Victory ? "Continue"
                                                                                           : "End Adventure");
            continueButton_->setVisible(true);
        }
    }
    refreshParticipantTable();
    auto rating = encounter_.difficulty();
    bool anyMonster = std::any_of(encounter_.combatants().begin(), encounter_.combatants().end(),
                                  [](const dnd::Combatant& c) { return !dnd::party_side(c); });
    difficultyLabel_->setText(anyMonster ? "Difficulty: " + q(rating.text()) : QString("Add monsters to fight."));
    // Color by palette, and only when the rating changes: re-applying a style
    // sheet on every refresh re-polishes the label, which crashed Qt 6.4.
    if (rating.difficulty != shownDifficulty_) {
        shownDifficulty_ = rating.difficulty;
        QPalette palette = difficultyLabel_->palette();
        palette.setColor(QPalette::WindowText, rating.difficulty == dnd::Difficulty::Deadly ? QColor(176, 0, 32)
                                               : rating.difficulty == dnd::Difficulty::Hard ? QColor(192, 80, 0)
                                                                                            : palette.color(QPalette::Text));
        difficultyLabel_->setPalette(palette);
        QFont font = difficultyLabel_->font();
        font.setBold(rating.difficulty == dnd::Difficulty::Deadly);
        difficultyLabel_->setFont(font);
    }
    takeActionButton_->setEnabled(encounter_.awaiting_action() || encounter_.bonus_action_available());
    refreshBonusControls();
    refreshLootPanel();

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
    // Cast a Spell and Read Scroll share the spell controls.
    bool scrolls = actionCombo_->currentIndex() == ReadScrollAction;
    if (scrolls != showingScrolls_) refreshSpellControls();
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
        case LayOnHandsAction:
            if (layOnHandsTargetCombo_->count() == 0) {
                log("Your Lay on Hands pool is empty until a long rest.");
                return;
            }
            acted = encounter_.lay_on_hands(layOnHandsTargetCombo_->currentData().toInt(), layOnHandsSpin_->value());
            break;
        case StabilizeAction:
            if (stabilizeCombo_->count() == 0) {
                log("No one is dying -- choose another action.");
                return;
            }
            acted = encounter_.stabilize(stabilizeCombo_->currentData().toInt());
            break;
        case HelpAction:
            if (helpCombo_->count() == 0) {
                log("No ally can be helped -- choose another action.");
                return;
            }
            acted = encounter_.help(helpCombo_->currentData().toInt());
            break;
        case ReadScrollAction: {
            const dnd::Spell* spell = selectedSpell();
            if (!spell) {
                log("No scrolls to read -- choose another action.");
                return;
            }
            std::vector<int> targets;
            for (int row = 0; row < spellTargetList_->count(); ++row) {
                if (spellTargetList_->item(row)->checkState() == Qt::Checked) {
                    targets.push_back(spellTargetList_->item(row)->data(Qt::UserRole).toInt());
                }
            }
            acted = encounter_.read_scroll(spellCombo_->currentData().toInt(), targets);
            break;
        }
        case CastSpellAction: {
            const dnd::Spell* spell = selectedSpell();
            std::vector<int> targets;
            for (int row = 0; row < spellTargetList_->count(); ++row) {
                if (spellTargetList_->item(row)->checkState() == Qt::Checked) {
                    targets.push_back(spellTargetList_->item(row)->data(Qt::UserRole).toInt());
                }
            }
            int slot = spell && spell->level > 0 ? spellSlotCombo_->currentData().toInt() : 0;
            auto metamagic = static_cast<dnd::Metamagic>(metamagicCombo_->isVisible() ? metamagicCombo_->currentData().toInt()
                                                                                    : 0);
            acted = encounter_.cast_spell(spell ? spell->spell_id : 0, slot, targets, metamagic);
            break;
        }
        default:
            if (targetCombo_->count() == 0) return;
        {
            dnd::AttackOptions options;
            options.reckless = recklessBox_->isVisible() && recklessBox_->isChecked();
            options.stunning_strike = stunningBox_->isVisible() && stunningBox_->isChecked();
            options.inspired = inspirationBox_->isVisible() && inspirationBox_->isChecked();
            options.divine_smite_slot = smiteCombo_->isVisible() ? smiteCombo_->currentData().toInt() : 0;
            acted = encounter_.attack(targetCombo_->currentData().toInt(), attackBonusSpin_->value(),
                                      damageDiceCountSpin_->value(), damageDiceSidesSpin_->value(),
                                      damageBonusSpin_->value(), options);
            break;
        }
    }

    processEvents();
    if (acted) afterCharacterActed();
    if (encounter_.status() != dnd::EncounterStatus::InProgress) onTurnStarted();
    refreshState();
}

void CombatEncounterDialog::afterCharacterActed() {
    updateAttackFieldsForCurrentTurn();  // Wild Shape swaps the attack
    refreshFeatureControls();
    refreshTargetCombo();
    refreshPotionCombo();
    refreshStabilizeCombo();
    refreshHelpCombo();
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

    // Channel Divinity options (an action): Turn Undead, Preserve Life, Sacred Weapon, ...
    channelCombo_->clear();
    for (const auto& option : encounter_.channel_divinity_options()) {
        channelCombo_->addItem(q(option.label), static_cast<int>(option.kind));
        channelCombo_->setItemData(channelCombo_->count() - 1, q(option.description), Qt::ToolTipRole);
    }
    bool channel = channelCombo_->count() > 0;
    channelRow_->setVisible(channel);
    if (auto* label = attackFormLayout_->labelForField(channelRow_)) label->setVisible(channel);

    wildShapeCombo_->clear();
    for (const auto& beast : encounter_.wild_shape_forms()) {
        QString cr = beast.challenge_rating == 0.25 ? "1/4" : beast.challenge_rating == 0.5 ? "1/2"
                                                                                              : QString::number(beast.challenge_rating);
        wildShapeCombo_->addItem(QString("%1 (CR %2, %3 HP, AC %4)")
                                     .arg(q(beast.name), cr)
                                     .arg(beast.hit_points)
                                     .arg(beast.armor_class),
                                 q(beast.name));
    }
    bool shapes = wildShapeCombo_->count() > 0;
    wildShapeRow_->setVisible(shapes);
    if (auto* label = attackFormLayout_->labelForField(wildShapeRow_)) label->setVisible(shapes);
}

void CombatEncounterDialog::onWildShapeClicked() {
    if (wildShapeCombo_->count() == 0) return;
    bool acted = encounter_.wild_shape(wildShapeCombo_->currentData().toString().toStdString());
    processEvents();
    if (acted) afterCharacterActed();
    if (encounter_.status() != dnd::EncounterStatus::InProgress) onTurnStarted();
    updateAttackFieldsForCurrentTurn();
    refreshState();
}

void CombatEncounterDialog::onChannelClicked() {
    if (channelCombo_->count() == 0) return;
    bool acted = encounter_.channel_divinity(static_cast<dnd::Encounter::Channel>(channelCombo_->currentData().toInt()));
    processEvents();
    if (acted) afterCharacterActed();
    if (encounter_.status() != dnd::EncounterStatus::InProgress) onTurnStarted();
    refreshState();
}

void CombatEncounterDialog::refreshBonusTargets() {
    bonusTargetCombo_->clear();
    int index = bonusCombo_->currentIndex();
    bool needsTarget = index >= 0 && index < static_cast<int>(bonusTargets_.size()) &&
                       bonusTargets_[static_cast<size_t>(index)] != dnd::Encounter::BonusTarget::None;
    bonusTargetCombo_->setVisible(needsTarget);
    if (!needsTarget) return;
    auto kind = bonusTargets_[static_cast<size_t>(index)];
    for (int target : encounter_.bonus_action_targets(kind)) {
        if (kind == dnd::Encounter::BonusTarget::SlotLevel) {
            bonusTargetCombo_->addItem(QString("%1-level slot (%2 points)").arg(ordinal(target)).arg(dnd::slot_creation_cost(target)),
                                       target);
            continue;
        }
        if (kind == dnd::Encounter::BonusTarget::Potion) {  // item ids: name the potion
            const dnd::Combatant* current = encounter_.current();
            QString name = "Potion";
            if (const auto* pack = current ? encounter_.pack_of(current->character_id) : nullptr) {
                for (const auto& entry : *pack) {
                    if (entry.item.item_id == target) name = QString("%1 (%2)").arg(q(entry.item.name)).arg(entry.quantity);
                }
            }
            bonusTargetCombo_->addItem(name, target);
            continue;
        }
        bonusTargetCombo_->addItem(q(encounter_.combatants()[static_cast<size_t>(target)].name), target);
    }
}

void CombatEncounterDialog::refreshFeatureControls() {
    recklessBox_->setChecked(false);
    stunningBox_->setChecked(false);
    inspirationBox_->setChecked(false);

    // Back to "No smite" after every attack, so slots are never spent by accident.
    smiteCombo_->clear();
    smiteCombo_->addItem("No smite", 0);
    for (int level : encounter_.divine_smite_slots()) {
        smiteCombo_->addItem(QString("%1-level slot: +%2d8 radiant on a hit")
                                 .arg(ordinal(level))
                                 .arg(dnd::divine_smite_dice(level)),
                             level);
    }

    layOnHandsTargetCombo_->blockSignals(true);
    layOnHandsTargetCombo_->clear();
    for (int index : encounter_.lay_on_hands_targets()) {
        const auto& target = encounter_.combatants()[static_cast<size_t>(index)];
        layOnHandsTargetCombo_->addItem(
            QString("%1 (%2/%3 HP)").arg(q(target.name)).arg(target.current_hp).arg(target.max_hp), index);
    }
    layOnHandsTargetCombo_->blockSignals(false);
    refreshLayOnHandsAmount();
}

void CombatEncounterDialog::refreshLayOnHandsAmount() {
    int pool = encounter_.lay_on_hands_pool();
    layOnHandsSpin_->setRange(pool > 0 ? 1 : 0, pool);
    int index = layOnHandsTargetCombo_->currentData().toInt();
    if (pool > 0 && layOnHandsTargetCombo_->count() > 0 && index >= 0 &&
        index < static_cast<int>(encounter_.combatants().size())) {
        const auto& target = encounter_.combatants()[static_cast<size_t>(index)];
        layOnHandsSpin_->setValue(std::clamp(target.max_hp - target.current_hp, 1, pool));  // enough to fill up
    }
    layOnHandsInfoLabel_->setText(pool > 0 ? QString("%1 HP left in your pool.").arg(pool)
                                           : QString("Your pool is empty until a long rest."));
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
    bool show_reckless = is_attack && encounter_.can_attack_recklessly();
    bool show_smite = is_attack && smiteCombo_->count() > 1;
    bool show_stunning = is_attack && encounter_.can_stunning_strike();
    for (auto [field, visible] : {std::pair<QWidget*, bool>{recklessBox_, show_reckless}, {smiteCombo_, show_smite},
                                  {stunningBox_, show_stunning},
                                  {inspirationBox_, is_attack && encounter_.has_inspiration()}}) {
        field->setVisible(visible);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(visible);
    }
    bool is_lay_on_hands = actionCombo_->currentIndex() == LayOnHandsAction;
    for (auto* field : {static_cast<QWidget*>(layOnHandsTargetCombo_), static_cast<QWidget*>(layOnHandsSpin_),
                        static_cast<QWidget*>(layOnHandsInfoLabel_)}) {
        field->setVisible(is_lay_on_hands);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(is_lay_on_hands);
    }

    bool is_potion = actionCombo_->currentIndex() == DrinkPotionAction;
    potionCombo_->setVisible(is_potion);
    if (auto* row_label = attackFormLayout_->labelForField(potionCombo_)) row_label->setVisible(is_potion);

    bool is_stabilize = actionCombo_->currentIndex() == StabilizeAction;
    for (auto* field : {static_cast<QWidget*>(stabilizeCombo_), static_cast<QWidget*>(stabilizeInfoLabel_)}) {
        field->setVisible(is_stabilize);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(is_stabilize);
    }

    bool is_help = actionCombo_->currentIndex() == HelpAction;
    helpCombo_->setVisible(is_help);
    if (auto* row_label = attackFormLayout_->labelForField(helpCombo_)) row_label->setVisible(is_help);

    bool is_scroll = actionCombo_->currentIndex() == ReadScrollAction;
    bool is_spell = actionCombo_->currentIndex() == CastSpellAction || is_scroll;
    const dnd::Spell* spell = is_spell ? selectedSpell() : nullptr;
    bool show_slot = spell && spell->level > 0 && !is_scroll;  // a scroll casts at its own level
    if (auto* label = qobject_cast<QLabel*>(attackFormLayout_->labelForField(spellCombo_))) {
        label->setText(is_scroll ? "Scroll" : "Spell");
    }
    bool show_targets = spell && !dnd::spell_targets_nobody(*spell);
    for (auto [field, visible] : {std::pair<QWidget*, bool>{spellCombo_, is_spell}, {spellSlotCombo_, show_slot},
                                  {spellTargetList_, show_targets}, {spellInfoLabel_, is_spell}}) {
        field->setVisible(visible);
        if (auto* row_label = attackFormLayout_->labelForField(field)) row_label->setVisible(visible);
    }

    bool show_metamagic = actionCombo_->currentIndex() == CastSpellAction && metamagicCombo_->count() > 0;
    metamagicCombo_->setVisible(show_metamagic);
    if (auto* label = attackFormLayout_->labelForField(metamagicCombo_)) label->setVisible(show_metamagic);

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
        case HelpAction: takeActionButton_->setText("Help Ally"); break;
        case ReadScrollAction:
            takeActionButton_->setText(spell && spell->casting_time == dnd::CastingTime::BonusAction
                                           ? "Read Scroll (bonus action)"
                                           : "Read Scroll");
            break;
        case LayOnHandsAction: takeActionButton_->setText("Lay on Hands"); break;
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
        auto* name = new QTableWidgetItem(
            combatant.druid_form ? QString("%1 (%2)").arg(q(combatant.name), q(combatant.druid_form->beast))
                                 : q(combatant.name));
        if (combatant.ally) {
            name->setText(name->text() + " — ally");
            name->setToolTip("Fights on your side, run by the monster AI.");
        }
        participantTable_->setItem(row, 0, name);
        participantTable_->setItem(row, 1, new QTableWidgetItem(QString::number(combatant.initiative)));
        participantTable_->setItem(row, 2, new QTableWidgetItem(QString::number(combatant.armor_class)));
        participantTable_->setItem(
            row, 3, new QTableWidgetItem(QString("%1/%2%3")
                                             .arg(combatant.current_hp)
                                             .arg(combatant.max_hp)
                                             .arg(combatant.temp_hp > 0 ? QString(" +%1 temp").arg(combatant.temp_hp)
                                                                        : QString())));
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

void CombatEncounterDialog::refreshHelpCombo() {
    helpCombo_->clear();
    for (int index : encounter_.help_targets()) {
        helpCombo_->addItem(q(encounter_.combatants()[static_cast<size_t>(index)].name), index);
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
    showingScrolls_ = actionCombo_->currentIndex() == ReadScrollAction;
    spellCombo_->blockSignals(true);
    spellCombo_->clear();
    castableSpells_.clear();
    if (showingScrolls_) {
        // The scrolls in the pack, each with the spell it casts (castableSpells_ stays parallel).
        for (const auto& entry : encounter_.scrolls()) {
            const dnd::Spell* spell = encounter_.scroll_spell(entry.item);
            auto reading = encounter_.scroll_reading(entry.item);
            QString note = !reading || !reading->readable ? " — can't read"
                           : reading->needs_check ? QString(" — DC %1 check").arg(reading->check_dc)
                                                  : QString();
            castableSpells_.push_back(*spell);
            spellCombo_->addItem(QString("%1 × %2%3").arg(entry.quantity).arg(q(entry.item.name), note),
                                 entry.item.item_id);
            spellCombo_->setItemData(spellCombo_->count() - 1, q(spell->description), Qt::ToolTipRole);
        }
        spellCombo_->blockSignals(false);
        onSpellChanged();
        return;
    }
    castableSpells_ = encounter_.castable_spells();
    for (const auto& spell : castableSpells_) {
        QString level = spell.level == 0 ? "cantrip" : ordinal(spell.level);
        if (spell.casting_time == dnd::CastingTime::BonusAction) level += ", bonus action";
        spellCombo_->addItem(QString("%1 (%2)").arg(q(spell.name), level), spell.spell_id);
        spellCombo_->setItemData(spellCombo_->count() - 1, q(spell.description), Qt::ToolTipRole);
    }
    spellCombo_->blockSignals(false);
    onSpellChanged();
}

void CombatEncounterDialog::refreshMetamagic() {
    metamagicCombo_->clear();
    const dnd::Spell* spell = selectedSpell();
    bool casting = actionCombo_->currentIndex() == CastSpellAction && spell;
    if (casting) {
        int slot = spell->level == 0 ? 0 : spellSlotCombo_->currentData().toInt();
        for (const auto& [metamagic, cost] : encounter_.metamagic_options(*spell, slot)) {
            if (metamagicCombo_->count() == 0) metamagicCombo_->addItem("None", 0);
            metamagicCombo_->addItem(QString("%1 (%2 %3)")
                                         .arg(dnd::metamagic_name(metamagic))
                                         .arg(cost)
                                         .arg(cost == 1 ? "point" : "points"),
                                     static_cast<int>(metamagic));
            metamagicCombo_->setItemData(metamagicCombo_->count() - 1, dnd::metamagic_description(metamagic),
                                         Qt::ToolTipRole);
        }
    }
    bool show = metamagicCombo_->count() > 0;
    metamagicCombo_->setVisible(show);
    if (auto* label = attackFormLayout_->labelForField(metamagicCombo_)) label->setVisible(show);
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
    refreshMetamagic();
}

void CombatEncounterDialog::refreshSpellInfo() {
    const dnd::Spell* spell = selectedSpell();
    if (!spell) {
        const dnd::Combatant* current = encounter_.current();
        QString why = current && current->armor_not_proficient
            ? "Can't cast spells while wearing armor you aren't proficient with (take it off in the Inventory)."
            : showingScrolls_ ? "No spell scrolls in the pack."
                              : "No spells to cast (none known, or no slots left).";
        bool casting = actionCombo_->currentIndex() == CastSpellAction ||
                       actionCombo_->currentIndex() == ReadScrollAction;
        spellInfoLabel_->setText(spellCombo_->count() == 0 && casting ? why : "");
        return;
    }
    const dnd::Combatant* current = encounter_.current();
    const dnd::Character* character = current ? encounter_.character(current->character_id) : nullptr;
    const dnd::ClassInfo* info = character ? dnd::find_class_info_by_id(classes_, character->class_id) : nullptr;
    if (!character || !info) return;

    auto caster = dnd::caster_stats(*character, *info);
    int slot = spell->level == 0 ? 0 : spellSlotCombo_->currentData().toInt();
    QString scrollNote;
    if (showingScrolls_) {  // the scroll's own numbers, at its own level
        slot = spell->level;
        dnd::Item scroll;
        scroll.scroll_spell_id = spell->spell_id;
        if (auto reading = encounter_.scroll_reading(scroll)) {
            if (!reading->readable) {
                spellInfoLabel_->setText(q(reading->reason));
                return;
            }
            caster.spell_attack_bonus = reading->attack_bonus;
            caster.spell_save_dc = reading->save_dc;
            scrollNote = reading->needs_check
                ? QString(" Above your highest spell slot: a DC %1 spellcasting check first, or the scroll is wasted.")
                      .arg(reading->check_dc)
                : QString();
        }
    }
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
        case dnd::SpellResolution::Lasting: {
            QString summary = q(dnd::spell_summary(*spell));
            text = summary.left(1).toUpper() + summary.mid(1) + ". It lasts past this fight.";
            break;
        }
        case dnd::SpellResolution::Revive:
            text = QString("A character who died in this fight returns with 1 HP.%1")
                       .arg(spell->consumed_item.empty() || showingScrolls_
                                ? QString()
                                : QString(" Uses up %1.").arg(q(spell->consumed_item)));
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
    spellInfoLabel_->setText(text + scrollNote);
}

void CombatEncounterDialog::log(const QString& line) {
    logView_->appendPlainText(line);
}
