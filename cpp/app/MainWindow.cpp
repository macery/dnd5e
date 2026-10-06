#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "AdventureDialogs.hpp"
#include "AdventurePickerDialog.hpp"
#include "CampaignDialog.hpp"
#include "NpcDialog.hpp"
#include "CharacterCreationWizard.hpp"
#include "CharacterEditorDialog.hpp"
#include "CharacterSheetDialog.hpp"
#include "CharacterTableModel.hpp"
#include "CombatEncounterDialog.hpp"
#include "DiceRollerWidget.hpp"
#include "InitiativeTrackerDialog.hpp"
#include "ImportDialog.hpp"
#include "InventoryDialog.hpp"
#include "SceneDialog.hpp"
#include "ShopDialog.hpp"
#include "LevelUpDialog.hpp"
#include "MonsterListDialog.hpp"
#include "SpellbookDialog.hpp"
#include "ClassOptionsDialog.hpp"
#include "SubclassDialog.hpp"
#include "WindowSizing.hpp"

#include "dnd/adventure.hpp"
#include "dnd/adventure_run.hpp"
#include "dnd/campaign.hpp"
#include "dnd/encounter_difficulty.hpp"
#include "dnd/character_io.hpp"
#include "dnd/class_features.hpp"
#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"
#include "dnd/scenes.hpp"
#include "dnd/spellbook.hpp"
#include "dnd/subclass.hpp"
#include "dnd/spellcasting.hpp"

MainWindow::MainWindow(dnd::Repository& repository, QWidget* parent)
    : QMainWindow(parent), repository_(repository) {
    setWindowTitle("DnD Engine");

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    // Rows of buttons wrap when the window is narrow (FlowLayout), so the
    // main window fits small screens and wider fonts (Windows).
    // The active campaign scopes play (docs/campaigns.md).
    auto* campaignRow = new FlowLayout();
    auto* campaignLabel = new QLabel("Campaign:", central);
    campaignCombo_ = new QComboBox(central);
    campaignCombo_->setMinimumWidth(200);
    campaignCombo_->setToolTip("The table you're playing at. Fights, adventure parties, trading, and the Temple use "
                               "only its characters, and its adventures resume separately. \"No campaign\" opens "
                               "everything to every character.");
    auto* campaignsButton = new QPushButton("Campaigns…", central);
    campaignsButton->setToolTip("Create campaigns and choose who plays in each: characters from any players.");
    campaignRow->addWidget(campaignLabel);
    campaignRow->addWidget(campaignCombo_);
    auto* npcsButton = new QPushButton("NPCs…", central);
    npcsButton->setToolTip("The campaign's people: the merchant, the priest, and everyone met in adventures. Edit "
                           "who they are and how they feel about the party.");
    campaignRow->addWidget(campaignsButton);
    campaignRow->addWidget(npcsButton);
    rootLayout->addLayout(campaignRow);
    connect(campaignsButton, &QPushButton::clicked, this, &MainWindow::onCampaignsClicked);
    connect(npcsButton, &QPushButton::clicked, this, &MainWindow::onNpcsClicked);

    auto* playerRow = new FlowLayout();
    playerCombo_ = new QComboBox(central);
    newPlayerName_ = new QLineEdit(central);
    newPlayerName_->setPlaceholderText("New player name");
    addPlayerButton_ = new QPushButton("Add Player", central);
    editPlayerButton_ = new QPushButton("Edit Player", central);
    deletePlayerButton_ = new QPushButton("Delete Player", central);
    playerCombo_->setMinimumWidth(160);
    playerCombo_->setToolTip("Which characters the list shows: everyone in play (the active campaign's characters, "
                             "or every character with no campaign), or just one player's.");
    newPlayerName_->setMinimumWidth(160);
    playerRow->addWidget(playerCombo_);
    playerRow->addWidget(newPlayerName_);
    playerRow->addWidget(addPlayerButton_);
    playerRow->addWidget(editPlayerButton_);
    playerRow->addWidget(deletePlayerButton_);
    rootLayout->addLayout(playerRow);

    // Save files: share or move characters, and back up everything (docs/save-load.md).
    auto* fileRow = new FlowLayout();
    exportButton_ = new QPushButton("Export…", central);
    exportButton_->setToolTip("Save this player's characters -- or just the selected character -- to a file you "
                              "can back up, share, or import on another machine.");
    importButton_ = new QPushButton("Import…", central);
    importButton_->setToolTip("Load characters from an exported file. You'll see what will happen before anything "
                              "changes.");
    backupButton_ = new QPushButton("Back Up Database…", central);
    backupButton_->setToolTip("Save a complete copy of everything: every player, character, and item.");
    fileRow->addWidget(exportButton_);
    fileRow->addWidget(importButton_);
    fileRow->addWidget(backupButton_);
    rootLayout->addLayout(fileRow);

    characterModel_ = new CharacterTableModel(central);
    characterTable_ = new QTableView(central);
    characterTable_->setModel(characterModel_);
    characterTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    characterTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    characterTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Name
    rootLayout->addWidget(characterTable_, 1);

    auto* characterRow = new FlowLayout();
    addCharacterButton_ = new QPushButton("Add Character", central);
    editCharacterButton_ = new QPushButton("Edit Character", central);
    deleteCharacterButton_ = new QPushButton("Delete Character", central);
    characterRow->addWidget(addCharacterButton_);
    characterRow->addWidget(editCharacterButton_);
    characterRow->addWidget(deleteCharacterButton_);
    inventoryButton_ = new QPushButton("Inventory", central);
    inventoryButton_->setToolTip("View, add, drop, and use the selected character's items.");
    characterRow->addWidget(inventoryButton_);
    shopButton_ = new QPushButton("Shop…", central);
    shopButton_->setToolTip("Buy and sell gear for the selected character, give coins to another of the player's "
                            "characters, or pay a temple to raise the dead.");
    characterRow->addWidget(shopButton_);
    spellbookButton_ = new QPushButton("Spellbook", central);
    spellbookButton_->setToolTip("The selected character's spells and spell slots: learn spells, and cast healing "
                                 "or utility spells outside combat.");
    characterRow->addWidget(spellbookButton_);
    levelUpButton_ = new QPushButton("Level Up", central);
    levelUpButton_->setToolTip("Take a level the selected character's XP has earned: hit points, Ability Score "
                               "Improvements, new features, and new spells. The Level column shows \"3 → 4\" "
                               "when one is ready.");
    characterRow->addWidget(levelUpButton_);
    subclassButton_ = new QPushButton("Choose Subclass…", central);
    subclassButton_->setToolTip("Choose the selected character's subclass (Divine Domain, Martial Archetype, ...) "
                                "once their class reaches it. The Status column says \"Subclass due\".");
    characterRow->addWidget(subclassButton_);
    connect(subclassButton_, &QPushButton::clicked, this, &MainWindow::onChooseSubclassClicked);
    classOptionsButton_ = new QPushButton("Class Options…", central);
    classOptionsButton_->setToolTip("Choices a class makes as it levels: Metamagic, Eldritch Invocations, Pact Boon, "
                                    "Expertise, ... The Status column says \"Choices due\".");
    characterRow->addWidget(classOptionsButton_);
    connect(classOptionsButton_, &QPushButton::clicked, this, &MainWindow::onClassOptionsClicked);
    rootLayout->addLayout(characterRow);

    auto* combatRow = new FlowLayout();
    hpAmountSpin_ = new QSpinBox(central);
    hpAmountSpin_->setRange(1, 999);
    hpAmountSpin_->setValue(1);
    damageButton_ = new QPushButton("Damage", central);
    healButton_ = new QPushButton("Heal", central);
    shortRestButton_ = new QPushButton("Short Rest", central);
    shortRestButton_->setToolTip(
        "An hour's breather: spend one hit die (one per level, recovered on a long rest) to regain "
        "that die's roll + your Constitution modifier in HP. Warlocks also regain their spell slots.");
    longRestButton_ = new QPushButton("Long Rest", central);
    longRestButton_->setToolTip(
        "A full night's rest: regain all HP, all spell slots, and up to half your spent hit dice.");
    initiativeTrackerButton_ = new QPushButton("Initiative Tracker", central);
    monstersButton_ = new QPushButton("Monsters", central);
    combatEncounterButton_ = new QPushButton("Combat Encounter", central);
    soloAdventureButton_ = new QPushButton("Solo Adventure", central);
    combatRow->addWidget(hpAmountSpin_);
    combatRow->addWidget(damageButton_);
    combatRow->addWidget(healButton_);
    combatRow->addWidget(shortRestButton_);
    combatRow->addWidget(longRestButton_);
    rootLayout->addLayout(combatRow);

    // Fights on their own row, so the window stays narrow enough for small screens.
    auto* playRow = new FlowLayout();
    playRow->addWidget(initiativeTrackerButton_);
    playRow->addWidget(monstersButton_);
    playRow->addWidget(combatEncounterButton_);
    playRow->addWidget(soloAdventureButton_);
    rootLayout->addLayout(playRow);

    diceRoller_ = new DiceRollerWidget(central);
    rootLayout->addWidget(diceRoller_);

    setCentralWidget(central);
    fitToScreen(this, QSize(900, 600), "MainWindow");

    connect(playerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPlayerSelected);
    connect(addPlayerButton_, &QPushButton::clicked, this, &MainWindow::onAddPlayerClicked);
    connect(editPlayerButton_, &QPushButton::clicked, this, &MainWindow::onEditPlayerClicked);
    connect(deletePlayerButton_, &QPushButton::clicked, this, &MainWindow::onDeletePlayerClicked);
    connect(addCharacterButton_, &QPushButton::clicked, this, &MainWindow::onAddCharacterClicked);
    connect(editCharacterButton_, &QPushButton::clicked, this, &MainWindow::onEditCharacterClicked);
    connect(deleteCharacterButton_, &QPushButton::clicked, this, &MainWindow::onDeleteCharacterClicked);
    connect(inventoryButton_, &QPushButton::clicked, this, &MainWindow::onInventoryClicked);
    connect(shopButton_, &QPushButton::clicked, this, &MainWindow::onShopClicked);
    connect(spellbookButton_, &QPushButton::clicked, this, &MainWindow::onSpellbookClicked);
    connect(levelUpButton_, &QPushButton::clicked, this, &MainWindow::onLevelUpClicked);
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(importButton_, &QPushButton::clicked, this, &MainWindow::onImportClicked);
    connect(backupButton_, &QPushButton::clicked, this, &MainWindow::onBackupClicked);
    connect(characterTable_, &QTableView::doubleClicked, this, &MainWindow::onCharacterActivated);
    connect(damageButton_, &QPushButton::clicked, this, &MainWindow::onDamageClicked);
    connect(healButton_, &QPushButton::clicked, this, &MainWindow::onHealClicked);
    connect(shortRestButton_, &QPushButton::clicked, this, &MainWindow::onShortRestClicked);
    connect(longRestButton_, &QPushButton::clicked, this, &MainWindow::onLongRestClicked);
    connect(initiativeTrackerButton_, &QPushButton::clicked, this, &MainWindow::onInitiativeTrackerClicked);
    connect(monstersButton_, &QPushButton::clicked, this, &MainWindow::onMonstersClicked);
    connect(combatEncounterButton_, &QPushButton::clicked, this, &MainWindow::onCombatEncounterClicked);
    connect(soloAdventureButton_, &QPushButton::clicked, this, &MainWindow::onSoloAdventureClicked);

    reloadLookupTables();
    reloadCampaigns();
    connect(campaignCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onCampaignSelected);
    reloadPlayers();
}

int MainWindow::currentPlayerId() const {
    return playerCombo_->currentData().toInt();  // 0: "All players"
}

QString MainWindow::currentPlayerName() const {
    return playerCombo_->currentText();
}

void MainWindow::reloadPlayers() {
    int selected = playerCombo_->count() > 0 ? currentPlayerId() : 0;
    auto players = repository_.get_players();
    playerCombo_->blockSignals(true);
    playerCombo_->clear();
    playerCombo_->addItem("All players", 0);
    for (const auto& player : players) {
        playerCombo_->addItem(QString::fromStdString(player.player_name), player.player_id);
    }
    playerCombo_->setCurrentIndex(std::max(0, playerCombo_->findData(selected)));
    playerCombo_->blockSignals(false);
    characterModel_->setPlayers(players);
    reloadCharactersForCurrentPlayer();
}

int MainWindow::playerForAction(const QString& action) {
    if (currentPlayerId() != 0) return currentPlayerId();
    auto players = repository_.get_players();
    if (players.empty()) {
        QMessageBox::information(this, action, "Add a player first.");
        return 0;
    }
    if (players.size() == 1) return players.front().player_id;
    QModelIndex index = characterTable_->currentIndex();
    if (action == "Export" && index.isValid()) return characterModel_->characterAt(index.row()).player_id;
    QStringList names;
    for (const auto& player : players) names << QString::fromStdString(player.player_name);
    bool ok = false;
    QString name = QInputDialog::getItem(this, action, "Which player?", names, 0, false, &ok);
    if (!ok) return 0;
    for (const auto& player : players) {
        if (QString::fromStdString(player.player_name) == name) return player.player_id;
    }
    return 0;
}

void MainWindow::reloadLookupTables() {
    classes_ = repository_.get_classes();
    races_ = repository_.get_races();
    alignments_ = repository_.get_alignments();
    characterModel_->setClasses(classes_);
    characterModel_->setRaces(races_);
    characterModel_->setAlignments(alignments_);
}

void MainWindow::reloadCampaigns() {
    auto campaigns = repository_.get_campaigns();
    int active = repository_.active_campaign_id();
    QSignalBlocker block(campaignCombo_);
    campaignCombo_->clear();
    campaignCombo_->addItem("No campaign (everyone plays)", 0);
    for (const auto& campaign : campaigns) {
        campaignCombo_->addItem(QString::fromStdString(campaign.name), campaign.campaign_id);
    }
    campaignCombo_->setCurrentIndex(std::max(0, campaignCombo_->findData(active)));
    characterModel_->setCampaigns(campaigns);
    std::string name = dnd::active_campaign_name(repository_);
    setWindowTitle(name.empty() ? QString("DnD Engine") : QString("DnD Engine — %1").arg(QString::fromStdString(name)));
}

void MainWindow::onCampaignSelected(int /*index*/) {
    repository_.set_active_campaign(campaignCombo_->currentData().toInt());
    reloadCampaigns();
    reloadCharactersForCurrentPlayer();  // the list shows who's in play
}

void MainWindow::onNpcsClicked() {
    NpcDialog dialog(repository_, this);
    dialog.exec();
}

void MainWindow::onCampaignsClicked() {
    CampaignDialog dialog(repository_, this);
    dialog.exec();
    reloadCampaigns();
    reloadCharactersForCurrentPlayer();
}

void MainWindow::reloadCharactersForCurrentPlayer() {
    // Everyone in play (the active campaign's characters, or all of them),
    // narrowed to one player when the filter names one.
    std::vector<dnd::Character> shown;
    for (auto& character : dnd::characters_in_play(repository_)) {
        if (currentPlayerId() == 0 || character.player_id == currentPlayerId()) shown.push_back(std::move(character));
    }
    characterModel_->setCharacters(std::move(shown));
    bool onePlayer = currentPlayerId() != 0;
    editPlayerButton_->setEnabled(onePlayer);
    deletePlayerButton_->setEnabled(onePlayer);
    QString why = onePlayer ? QString() : QString("Choose a player in the list first.");
    editPlayerButton_->setToolTip(why);
    deletePlayerButton_->setToolTip(why);
}

void MainWindow::onPlayerSelected(int /*index*/) {
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onAddPlayerClicked() {
    auto name = newPlayerName_->text().trimmed();
    if (name.isEmpty()) return;

    repository_.add_player(name.toStdString());
    newPlayerName_->clear();
    reloadPlayers();
}

void MainWindow::onEditPlayerClicked() {
    if (currentPlayerId() == 0) return;

    bool ok = false;
    QString newName = QInputDialog::getText(this, "Edit Player", "Name:", QLineEdit::Normal, currentPlayerName(),
                                             &ok);
    if (!ok) return;
    newName = newName.trimmed();
    if (newName.isEmpty()) return;

    dnd::Player player;
    player.player_id = currentPlayerId();
    player.player_name = newName.toStdString();
    repository_.update_player(player);
    reloadPlayers();
}

void MainWindow::onDeletePlayerClicked() {
    if (currentPlayerId() == 0) return;

    int player_id = currentPlayerId();
    QString name = currentPlayerName();
    size_t character_count = repository_.get_characters_by_player(player_id).size();

    QString message = character_count > 0
        ? QString("Delete %1? This will also delete %2 character(s).").arg(name).arg(character_count)
        : QString("Delete %1?").arg(name);

    if (QMessageBox::question(this, "Delete Player", message) != QMessageBox::Yes) return;

    repository_.delete_player(player_id);
    playerCombo_->setCurrentIndex(0);  // back to "All players"
    reloadPlayers();
}

void MainWindow::onAddCharacterClicked() {
    int playerId = playerForAction("Add Character");
    if (playerId == 0) return;

    int before = static_cast<int>(repository_.get_characters().size());
    CharacterCreationWizard wizard(repository_, playerId, this);
    if (wizard.exec() != QDialog::Accepted) return;
    auto characters = repository_.get_characters();
    if (static_cast<int>(characters.size()) > before) {
        const dnd::Character* newest = &characters.front();
        for (const auto& character : characters) {
            if (character.character_id > newest->character_id) newest = &character;
        }
        // In a campaign, a new character joins it, so it shows up and can play.
        int campaign = repository_.active_campaign_id();
        if (campaign != 0) repository_.set_character_campaign(newest->character_id, campaign);
        offerSubclass(*newest);  // Clerics, Sorcerers, and Warlocks choose at level 1
        offerClassOptions(reloaded(*newest));  // a Rogue's Expertise
    }
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onEditCharacterClicked() {
    QModelIndex index = characterTable_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "Edit Character", "Select a character first.");
        return;
    }

    const dnd::Character& existing = characterModel_->characterAt(index.row());
    CharacterEditorDialog dialog(classes_, races_, alignments_, &existing, this);
    if (dialog.exec() == QDialog::Accepted) {
        dnd::Character updated = dialog.character();
        updated.character_id = existing.character_id;
        updated.player_id = existing.player_id;
        // Not editable in the dialog -- earned/spent through play only.
        updated.hit_dice_used = existing.hit_dice_used;
        updated.background = existing.background;
        updated.spell_slots_used = existing.spell_slots_used;
        updated.feature_uses_spent = existing.feature_uses_spent;
        updated.dead = existing.dead;
        updated.revival_penalty = existing.revival_penalty;
        updated.may_prepare = existing.may_prepare;
        updated.subclass = existing.subclass;
        updated.subclass_choice = existing.subclass_choice;
        updated.exhaustion = existing.exhaustion;
        updated.temp_hp = existing.temp_hp;
        updated.class_options = existing.class_options;
        updated.max_hp_reduction = existing.max_hp_reduction;
        updated.inspiration = existing.inspiration;
        updated.coins = existing.coins;
        repository_.update_character(updated);
        reloadCharactersForCurrentPlayer();
    }
}

void MainWindow::onDeleteCharacterClicked() {
    QModelIndex index = characterTable_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "Delete Character", "Select a character first.");
        return;
    }

    const dnd::Character& character = characterModel_->characterAt(index.row());
    QString name = QString::fromStdString(character.character_name);
    if (QMessageBox::question(this, "Delete Character", QString("Delete %1?").arg(name)) != QMessageBox::Yes) {
        return;
    }

    repository_.delete_character(character.character_id);
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onCharacterActivated(const QModelIndex& index) {
    if (!index.isValid()) return;

    const dnd::Character& character = characterModel_->characterAt(index.row());
    CharacterSheetDialog dialog(character, classes_, races_, alignments_,
                                repository_.get_inventory(character.character_id),
                                dnd::spells_ready(repository_, character), this);
    dialog.exec();
}

const dnd::Character* MainWindow::selectedCharacter(const QString& action) {
    QModelIndex index = characterTable_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, action, "Select a character first.");
        return nullptr;
    }
    return &characterModel_->characterAt(index.row());
}

void MainWindow::adjustCurrentCharacterHp(int delta) {
    const dnd::Character* selected = selectedCharacter("Adjust HP");
    if (!selected) return;

    dnd::Character character = *selected;
    if (character.dead) {
        QMessageBox::information(this, "Adjust HP",
            QString("%1 is dead. A temple can raise them: see the Shop's Temple tab.").arg(QString::fromStdString(character.character_name)));
        return;
    }
    character.current_hp = std::clamp(character.current_hp + delta, 0, character.max_hp);
    repository_.update_character(character);
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onDamageClicked() {
    adjustCurrentCharacterHp(-hpAmountSpin_->value());
}

void MainWindow::onHealClicked() {
    adjustCurrentCharacterHp(hpAmountSpin_->value());
}

void MainWindow::onInventoryClicked() {
    const dnd::Character* selected = selectedCharacter("Inventory");
    if (!selected) return;

    InventoryDialog dialog(repository_, *selected, classes_, races_, this);
    dialog.exec();
    reloadCharactersForCurrentPlayer();  // using a potion changes HP
}

void MainWindow::onShopClicked() {
    const dnd::Character* selected = selectedCharacter("Shop");
    if (!selected) return;

    ShopDialog dialog(repository_, *selected, classes_, races_, this);
    dialog.exec();
    reloadCharactersForCurrentPlayer();  // coins, and anyone raised
}

void MainWindow::onSpellbookClicked() {
    const dnd::Character* selected = selectedCharacter("Spellbook");
    if (!selected) return;

    SpellbookDialog dialog(repository_, *selected, classes_, this);
    dialog.exec();
    reloadCharactersForCurrentPlayer();  // casting changes HP and slots
}

void MainWindow::onLevelUpClicked() {
    const dnd::Character* selected = selectedCharacter("Level Up");
    if (!selected) return;
    dnd::Character character = *selected;
    if (character.dead) {
        QMessageBox::information(this, "Level Up", QString::fromStdString(character.character_name) +
                                                       " is dead and can't level up.");
        return;
    }
    if (dnd::pending_levels(character) == 0) {
        QString message = character.level >= dnd::kMaxLevel
            ? QString("%1 is already level 20.").arg(QString::fromStdString(character.character_name))
            : QString("%1 has %2 XP and needs %3 for level %4.")
                  .arg(QString::fromStdString(character.character_name))
                  .arg(character.experience)
                  .arg(dnd::xp_threshold_for_level(character.level + 1))
                  .arg(character.level + 1);
        QMessageBox::information(this, "Level Up", message);
        return;
    }

    // One level per dialog; keep going while more are earned and the player finishes each.
    while (dnd::pending_levels(character) > 0) {
        LevelUpDialog dialog(repository_, character, classes_, this);
        if (dialog.exec() != QDialog::Accepted) break;
        character = dialog.character();
        offerSubclass(character);  // the level that brings a subclass
        character = reloaded(character);
        offerClassOptions(character);  // Metamagic, invocations, Expertise, ...
        character = reloaded(character);
    }
    reloadCharactersForCurrentPlayer();
}

bool MainWindow::offerSubclass(const dnd::Character& character) {
    std::string className;
    for (const auto& entry : classes_) {
        if (entry.id == character.class_id) className = entry.name;
    }
    if (!dnd::subclass_due(character, className)) return false;
    SubclassDialog dialog(repository_, character, classes_, this);
    return dialog.exec() == QDialog::Accepted;
}

dnd::Character MainWindow::reloaded(const dnd::Character& character) const {
    for (const auto& fresh : repository_.get_characters()) {
        if (fresh.character_id == character.character_id) return fresh;
    }
    return character;
}

bool MainWindow::offerClassOptions(const dnd::Character& character) {
    std::string className;
    for (const auto& entry : classes_) {
        if (entry.id == character.class_id) className = entry.name;
    }
    auto spells = ClassOptionsDialog::spellContext(repository_, character);
    if (dnd::class_choices_due(character, className, spells).empty()) return false;
    ClassOptionsDialog dialog(repository_, character, classes_, this);
    return dialog.exec() == QDialog::Accepted;
}

void MainWindow::onClassOptionsClicked() {
    const dnd::Character* selected = selectedCharacter("Class Options");
    if (!selected) return;
    dnd::Character character = *selected;
    if (!offerClassOptions(character)) {
        QMessageBox::information(this, "Class Options",
            QString("%1 has no class choices to make right now.").arg(QString::fromStdString(character.character_name)));
    }
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onChooseSubclassClicked() {
    const dnd::Character* selected = selectedCharacter("Choose Subclass");
    if (!selected) return;
    dnd::Character character = *selected;
    std::string className;
    for (const auto& entry : classes_) {
        if (entry.id == character.class_id) className = entry.name;
    }
    if (!dnd::subclass_due(character, className)) {
        int level = dnd::subclass_level(className);
        QString message = !character.subclass.empty()
            ? QString("%1 already follows %2.").arg(QString::fromStdString(character.character_name),
                                                    QString::fromStdString(character.subclass))
            : level == 0 ? QString("%1 has no subclasses yet.").arg(QString::fromStdString(className))
                         : QString("%1s choose one at level %2.").arg(QString::fromStdString(className)).arg(level);
        QMessageBox::information(this, "Choose Subclass", message);
        return;
    }
    offerSubclass(character);
    reloadCharactersForCurrentPlayer();
}

namespace {

QString documentsFolder() {
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return folder.isEmpty() ? QDir::homePath() : folder;
}

// A safe file name from a player or character name ("Aria/Zed" -> "Aria_Zed").
QString safeFileName(QString name) {
    name.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");  // characters file systems reject
    return name.trimmed().isEmpty() ? QString("characters") : name.trimmed();
}

}  // namespace

void MainWindow::onExportClicked() {
    int playerId = playerForAction("Export");
    if (playerId == 0) return;
    QString playerName;
    for (const auto& player : repository_.get_players()) {
        if (player.player_id == playerId) playerName = QString::fromStdString(player.player_name);
    }
    const dnd::Character* selected = nullptr;
    QModelIndex index = characterTable_->currentIndex();
    if (index.isValid() && characterModel_->characterAt(index.row()).player_id == playerId) {
        selected = &characterModel_->characterAt(index.row());
    }

    // With a character selected, ask: the whole player, or just that character?
    bool justOne = false;
    if (selected) {
        QMessageBox ask(QMessageBox::Question, "Export",
                        QString("Export all of %1's characters, or just %2?")
                            .arg(playerName, QString::fromStdString(selected->character_name)),
                        QMessageBox::Cancel, this);
        auto* all = ask.addButton(QString("All of %1's").arg(playerName), QMessageBox::AcceptRole);
        auto* one = ask.addButton(QString("Just %1").arg(QString::fromStdString(selected->character_name)),
                                  QMessageBox::AcceptRole);
        ask.setDefaultButton(all);
        ask.exec();
        if (ask.clickedButton() == one) {
            justOne = true;
        } else if (ask.clickedButton() != all) {
            return;
        }
    }

    QString suggested = safeFileName(justOne ? QString::fromStdString(selected->character_name) : playerName);
    QString path = QFileDialog::getSaveFileName(this, "Export Characters",
                                                documentsFolder() + "/" + suggested + ".dnd5e.json",
                                                "Character files (*.dnd5e.json);;All files (*)");
    if (path.isEmpty()) return;

    dnd::CharacterExport file = justOne ? dnd::export_character(repository_, selected->character_id)
                                        : dnd::export_player(repository_, playerId);
    file.app_version = QCoreApplication::applicationVersion().toStdString();
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) ||
        out.write(QByteArray::fromStdString(dnd::to_json(file))) < 0) {
        QMessageBox::warning(this, "Export", QString("Couldn't write %1:\n%2").arg(path, out.errorString()));
        return;
    }
    QMessageBox::information(this, "Export",
                             QString("Exported %1 character%2 to:\n%3")
                                 .arg(file.characters.size())
                                 .arg(file.characters.size() == 1 ? "" : "s")
                                 .arg(path));
}

void MainWindow::onImportClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Import Characters", documentsFolder(),
                                                "Character files (*.dnd5e.json *.json);;All files (*)");
    if (path.isEmpty()) return;
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import", QString("Couldn't read %1:\n%2").arg(path, in.errorString()));
        return;
    }
    auto parsed = dnd::parse_character_file(in.readAll().toStdString());
    if (!parsed.file) {
        QMessageBox::warning(this, "Import", QString::fromStdString(parsed.error));
        return;
    }

    ImportDialog dialog(repository_, *parsed.file, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto& result = dialog.result();
    reloadPlayers();
    int row = playerCombo_->findData(result.player_id);
    if (row >= 0) playerCombo_->setCurrentIndex(row);
    QStringList lines;
    for (const auto& message : result.messages) lines << QString::fromStdString(message);
    QMessageBox::information(this, "Import", lines.join("\n"));
}

void MainWindow::onBackupClicked() {
    QString suggested = QString("dnd5e-backup-%1.db").arg(QDate::currentDate().toString(Qt::ISODate));
    QString path = QFileDialog::getSaveFileName(this, "Back Up Database", documentsFolder() + "/" + suggested,
                                                "Database backups (*.db);;All files (*)");
    if (path.isEmpty()) return;
    try {
        repository_.backup_to(path.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Back Up Database", error.what());
        return;
    }
    QMessageBox::information(this, "Back Up Database",
                             QString("Saved a complete copy of the database to:\n%1\n\nTo restore it, close the app "
                                     "and copy it over:\n%2")
                                 .arg(path, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                                                "/dnd.db"));
}

void MainWindow::onShortRestClicked() {
    const dnd::Character* selected = selectedCharacter("Short Rest");
    if (!selected) return;
    if (selected->dead) {
        QMessageBox::information(this, "Short Rest",
                                 QString("%1 is dead. Resting can't bring them back.")
                                     .arg(QString::fromStdString(selected->character_name)));
        return;
    }
    // Choose how many hit dice to spend; features, pact slots, and Arcane Recovery come back too.
    ShortRestDialog dialog(repository_, {*selected}, classes_, this);
    dialog.exec();
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onLongRestClicked() {
    const dnd::Character* selected = selectedCharacter("Long Rest");
    if (!selected) return;

    dnd::Character character = *selected;
    if (character.dead) {
        QMessageBox::information(this, "Long Rest",
            QString("%1 is dead. Resting can't bring them back.").arg(QString::fromStdString(character.character_name)));
        return;
    }
    dnd::take_long_rest(character);
    repository_.update_character(character);
    reloadCharactersForCurrentPlayer();
    QMessageBox::information(this, "Long Rest",
        QString("%1 is fully rested: %2/%3 HP, %4/%5 hit dice, all spell slots and class features.")
            .arg(QString::fromStdString(character.character_name))
            .arg(character.current_hp)
            .arg(character.max_hp)
            .arg(dnd::hit_dice_remaining(character))
            .arg(character.level));
}

std::vector<dnd::Character> MainWindow::charactersWhoCanFight(const QString& action) {
    auto characters = dnd::characters_in_play(repository_);  // the active campaign's, or everyone
    if (characters.empty() && repository_.active_campaign_id() != 0) {
        QMessageBox::information(this, action,
            "No one plays in this campaign yet. Add characters with Campaigns… (or choose \"No campaign\").");
        return {};
    }
    std::vector<dnd::Character> living;
    for (const auto& character : characters) {
        if (!character.dead) living.push_back(character);
    }
    if (living.empty() && !characters.empty()) {
        QMessageBox::information(this, action,
            "Every character is dead. A temple can raise them for 550 gp: see the Shop's Temple tab.");
    }
    return living;
}

void MainWindow::onInitiativeTrackerClicked() {
    auto characters = charactersWhoCanFight("Initiative Tracker");
    if (characters.empty() && !repository_.get_characters().empty()) return;
    InitiativeTrackerDialog dialog(characters, this);
    dialog.exec();
}

void MainWindow::onMonstersClicked() {
    MonsterListDialog dialog(repository_.get_monsters(), this);
    dialog.exec();
}

void MainWindow::onCombatEncounterClicked() {
    auto characters = charactersWhoCanFight("Combat Encounter");
    if (characters.empty() && !repository_.get_characters().empty()) return;
    CombatEncounterDialog dialog(repository_, characters, repository_.get_monsters(), this);
    dialog.exec();
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onSoloAdventureClicked() {
    auto living = charactersWhoCanFight("Solo Adventure");
    if (living.empty()) {
        if (repository_.get_characters().empty()) {
            QMessageBox::information(this, "Solo Adventure", "Create a character first.");
        }
        return;
    }

    // Ratings are for the current player's characters (or everyone, if they have none) --
    // in a campaign, for the whole roster.
    bool campaign = repository_.active_campaign_id() != 0;
    std::vector<int> partyLevels;
    for (const auto& character : living) {
        if (campaign || character.player_id == currentPlayerId()) partyLevels.push_back(character.level);
    }
    if (partyLevels.empty()) {
        for (const auto& character : living) partyLevels.push_back(character.level);
    }
    AdventurePickerDialog picker(partyLevels, repository_.get_monsters(), dnd::runs_in_play(repository_, true), this);
    if (picker.exec() != QDialog::Accepted) return;
    if (picker.isResume()) {
        playAdventure(picker.selectedRun());
        return;
    }

    const dnd::AdventureTemplate& adventure = picker.selectedAdventure();
    PartyDialog partyDialog(living, repository_.get_players(), classes_, campaign ? 0 : currentPlayerId(),
                            QString::fromStdString(adventure.title), this);
    if (partyDialog.exec() != QDialog::Accepted) return;

    dnd::AdventureRun run;
    run.title = adventure.title;
    run.party = partyDialog.partyIds();
    run.started_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    run.campaign_id = repository_.active_campaign_id();
    run.run_id = repository_.add_adventure_run(run);
    playAdventure(run);
}

void MainWindow::playAdventure(dnd::AdventureRun run) {
    const dnd::AdventureTemplate* adventure = dnd::find_adventure(run.title);
    if (!adventure) {
        QMessageBox::warning(this, "Solo Adventure", "This adventure isn't in this version of the app.");
        return;
    }
    QString title = QString::fromStdString(adventure->title);
    auto monsters = repository_.get_monsters();
    auto partyMembers = [&] {
        std::vector<dnd::Character> members;
        for (const auto& character : repository_.get_characters()) {
            bool inParty = std::find(run.party.begin(), run.party.end(), character.character_id) != run.party.end();
            if (inParty && !character.dead) members.push_back(character);
        }
        return members;
    };

    while (run.status == dnd::AdventureStatus::InProgress) {
        auto members = partyMembers();
        if (members.empty()) {  // everyone died (or was deleted) since
            run.status = dnd::AdventureStatus::Defeated;
            repository_.update_adventure_run(run);
            QMessageBox::information(this, title, "No one in the party can go on. The adventure is over.");
            break;
        }

        if (dnd::between_scenes(run)) {
            BetweenScenesDialog between(repository_, run, *adventure, classes_, this);
            between.exec();
            if (between.choice() == BetweenScenesDialog::Choice::StopForNow) {
                repository_.update_adventure_run(run);
                reloadCharactersForCurrentPlayer();
                return;  // resume from Solo Adventure
            }
            if (between.choice() == BetweenScenesDialog::Choice::Retreat) {
                dnd::retreat(run);
                repository_.update_adventure_run(run);
                QMessageBox::information(this, title,
                                         "The party retreats, keeping everything earned so far. The adventure is "
                                         "over.");
                break;
            }
            members = partyMembers();  // after rests and level-ups
        }

        const dnd::AdventureScene* scene = dnd::next_scene(run, *adventure);
        QString sceneTitle = adventure->scenes.size() == 1
                                 ? title
                                 : QString("%1 — %2: %3")
                                       .arg(title, QString::fromStdString(dnd::progress_text(run, *adventure)),
                                            QString::fromStdString(scene->title));
        if (scene->kind != dnd::SceneKind::Fight) {
            // A challenge, trap, choice, or conversation (docs/scenes-and-npcs.md).
            SceneDialog sceneDialog(repository_, run, *adventure, *scene, sceneTitle, this);
            bool continued = sceneDialog.exec() == QDialog::Accepted;
            repository_.update_adventure_run(run);
            if (!continued) {
                reloadCharactersForCurrentPlayer();
                return;  // resume from Solo Adventure
            }
            continue;
        }
        std::vector<int> levels;
        for (const auto& member : members) levels.push_back(member.level);
        AdventureFight fight;
        fight.scene = dnd::scale_scene(*scene, levels, monsters).scene;
        bool skip = false;
        fight.surprise = dnd::shape_next_fight(run, fight.scene, skip);  // what earlier scenes did to it
        if (skip) {
            dnd::record_fight(run, *adventure, true);
            repository_.update_adventure_run(run);
            QMessageBox::information(this, sceneTitle,
                                     QString("The party slips past %1 without a fight (and without its rewards).")
                                         .arg(QString::fromStdString(scene->title)));
            continue;
        }
        fight.victory_text = dnd::is_last_scene(run, *adventure) ? adventure->victory_text : std::string();
        fight.title = sceneTitle;

        CombatEncounterDialog combat(repository_, members, monsters, this, &fight);
        combat.exec();
        switch (combat.outcome()) {
            case dnd::EncounterStatus::Victory:
                dnd::record_fight(run, *adventure, true);
                repository_.update_adventure_run(run);
                break;
            case dnd::EncounterStatus::Defeat:
                dnd::record_fight(run, *adventure, false);
                repository_.update_adventure_run(run);
                QMessageBox::information(this, title, "The party has fallen. The adventure is over.");
                break;
            case dnd::EncounterStatus::InProgress:
                // Left mid-fight: the fight starts over on resume (the damage taken stays).
                repository_.update_adventure_run(run);
                QMessageBox::information(this, title,
                                         "You left the fight. The adventure is saved before it: use Solo Adventure "
                                         "to resume and fight it again.");
                reloadCharactersForCurrentPlayer();
                return;
        }
    }

    if (run.status == dnd::AdventureStatus::Finished && adventure->scenes.size() > 1) {
        QMessageBox::information(this, title,
                                 QString("Adventure complete!\n\n%1").arg(QString::fromStdString(adventure->victory_text)));
    }
    reloadCharactersForCurrentPlayer();
}
