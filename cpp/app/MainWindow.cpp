#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
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
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "AdventurePickerDialog.hpp"
#include "CharacterCreationWizard.hpp"
#include "CharacterEditorDialog.hpp"
#include "CharacterSheetDialog.hpp"
#include "CharacterTableModel.hpp"
#include "CombatEncounterDialog.hpp"
#include "DiceRollerWidget.hpp"
#include "InitiativeTrackerDialog.hpp"
#include "ImportDialog.hpp"
#include "InventoryDialog.hpp"
#include "LevelUpDialog.hpp"
#include "MonsterListDialog.hpp"
#include "SpellbookDialog.hpp"
#include "WindowSizing.hpp"

#include "dnd/adventure.hpp"
#include "dnd/character_io.hpp"
#include "dnd/class_features.hpp"
#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"
#include "dnd/spellcasting.hpp"

MainWindow::MainWindow(dnd::Repository& repository, QWidget* parent)
    : QMainWindow(parent), repository_(repository) {
    setWindowTitle("DnD Engine");

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    auto* playerRow = new QHBoxLayout();
    playerCombo_ = new QComboBox(central);
    newPlayerName_ = new QLineEdit(central);
    newPlayerName_->setPlaceholderText("New player name");
    addPlayerButton_ = new QPushButton("Add Player", central);
    editPlayerButton_ = new QPushButton("Edit Player", central);
    deletePlayerButton_ = new QPushButton("Delete Player", central);
    playerRow->addWidget(playerCombo_, 1);
    playerRow->addWidget(newPlayerName_, 1);
    playerRow->addWidget(addPlayerButton_);
    playerRow->addWidget(editPlayerButton_);
    playerRow->addWidget(deletePlayerButton_);
    rootLayout->addLayout(playerRow);

    // Save files: share or move characters, and back up everything (docs/save-load.md).
    auto* fileRow = new QHBoxLayout();
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
    fileRow->addStretch(1);
    rootLayout->addLayout(fileRow);

    characterModel_ = new CharacterTableModel(central);
    characterTable_ = new QTableView(central);
    characterTable_->setModel(characterModel_);
    characterTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    characterTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    characterTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Name
    rootLayout->addWidget(characterTable_, 1);

    auto* characterRow = new QHBoxLayout();
    addCharacterButton_ = new QPushButton("Add Character", central);
    editCharacterButton_ = new QPushButton("Edit Character", central);
    deleteCharacterButton_ = new QPushButton("Delete Character", central);
    characterRow->addWidget(addCharacterButton_);
    characterRow->addWidget(editCharacterButton_);
    characterRow->addWidget(deleteCharacterButton_);
    inventoryButton_ = new QPushButton("Inventory", central);
    inventoryButton_->setToolTip("View, add, drop, and use the selected character's items.");
    characterRow->addWidget(inventoryButton_);
    spellbookButton_ = new QPushButton("Spellbook", central);
    spellbookButton_->setToolTip("The selected character's spells and spell slots: learn spells, and cast healing "
                                 "or utility spells outside combat.");
    characterRow->addWidget(spellbookButton_);
    levelUpButton_ = new QPushButton("Level Up", central);
    levelUpButton_->setToolTip("Take a level the selected character's XP has earned: hit points, Ability Score "
                               "Improvements, new features, and new spells. The Level column shows \"3 → 4\" "
                               "when one is ready.");
    characterRow->addWidget(levelUpButton_);
    rootLayout->addLayout(characterRow);

    auto* combatRow = new QHBoxLayout();
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
    combatRow->addStretch(1);
    rootLayout->addLayout(combatRow);

    // Fights on their own row, so the window stays narrow enough for small screens.
    auto* playRow = new QHBoxLayout();
    playRow->addWidget(initiativeTrackerButton_);
    playRow->addWidget(monstersButton_);
    playRow->addWidget(combatEncounterButton_);
    playRow->addWidget(soloAdventureButton_);
    playRow->addStretch(1);
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
    reloadPlayers();
}

int MainWindow::currentPlayerId() const {
    return playerCombo_->currentData().toInt();
}

QString MainWindow::currentPlayerName() const {
    return playerCombo_->currentText();
}

void MainWindow::reloadPlayers() {
    playerCombo_->blockSignals(true);
    playerCombo_->clear();
    for (const auto& player : repository_.get_players()) {
        playerCombo_->addItem(QString::fromStdString(player.player_name), player.player_id);
    }
    playerCombo_->blockSignals(false);
    reloadCharactersForCurrentPlayer();
}

void MainWindow::reloadLookupTables() {
    classes_ = repository_.get_classes();
    races_ = repository_.get_races();
    alignments_ = repository_.get_alignments();
    characterModel_->setClasses(classes_);
    characterModel_->setRaces(races_);
    characterModel_->setAlignments(alignments_);
}

void MainWindow::reloadCharactersForCurrentPlayer() {
    if (playerCombo_->count() == 0) {
        characterModel_->setCharacters({});
        return;
    }
    characterModel_->setCharacters(repository_.get_characters_by_player(currentPlayerId()));
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
    if (playerCombo_->count() == 0) return;

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
    if (playerCombo_->count() == 0) return;

    int player_id = currentPlayerId();
    QString name = currentPlayerName();
    size_t character_count = repository_.get_characters_by_player(player_id).size();

    QString message = character_count > 0
        ? QString("Delete %1? This will also delete %2 character(s).").arg(name).arg(character_count)
        : QString("Delete %1?").arg(name);

    if (QMessageBox::question(this, "Delete Player", message) != QMessageBox::Yes) return;

    repository_.delete_player(player_id);
    reloadPlayers();
}

void MainWindow::onAddCharacterClicked() {
    if (playerCombo_->count() == 0) {
        QMessageBox::information(this, "Add Character", "Add a player first.");
        return;
    }

    CharacterCreationWizard wizard(repository_, currentPlayerId(), this);
    if (wizard.exec() == QDialog::Accepted) reloadCharactersForCurrentPlayer();
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
                                repository_.get_character_spells(character.character_id), this);
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
            QString("%1 is dead. Only revival magic can bring them back.").arg(QString::fromStdString(character.character_name)));
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
    }
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
    if (playerCombo_->count() == 0) {
        QMessageBox::information(this, "Export", "Add a player first.");
        return;
    }
    int playerId = currentPlayerId();
    QString playerName = currentPlayerName();
    const dnd::Character* selected = nullptr;
    QModelIndex index = characterTable_->currentIndex();
    if (index.isValid()) selected = &characterModel_->characterAt(index.row());

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

    dnd::Character character = *selected;
    QString name = QString::fromStdString(character.character_name);
    if (character.dead) {
        QMessageBox::information(this, "Short Rest", QString("%1 is dead. Resting can't bring them back.").arg(name));
        return;
    }

    // Pact slots and short-rest features come back on any short rest, hit
    // dice or not; a Wizard also uses Arcane Recovery if any slots are spent.
    bool slotsRestored = false;
    bool featuresRestored = false;
    QStringList recoveredSlots;
    if (const dnd::ClassInfo* info = dnd::find_class_info_by_id(classes_, character.class_id)) {
        auto slotsBefore = character.spell_slots_used;
        auto featuresBefore = character.feature_uses_spent;
        dnd::take_short_rest(character, *info);
        slotsRestored = character.spell_slots_used != slotsBefore;
        featuresRestored = character.feature_uses_spent != featuresBefore;

        auto recovered = dnd::use_arcane_recovery(character, info->name);
        for (int level = 1; level <= 9; ++level) {
            int count = recovered[static_cast<size_t>(level - 1)];
            if (count > 0) recoveredSlots << QString("%1 × %2-level").arg(count).arg(QString::fromStdString(dnd::ordinal(level)));
        }
    }

    int hit_die = dnd::hit_die_for_class_id(classes_, character.class_id);
    int regained = dnd::spend_hit_die(character, dice_, hit_die);
    if (regained < 0 && !slotsRestored && !featuresRestored && recoveredSlots.isEmpty()) {
        QMessageBox::information(this, "Short Rest",
            QString("%1 has no hit dice left. Take a long rest to recover them.").arg(name));
        return;
    }

    repository_.update_character(character);
    reloadCharactersForCurrentPlayer();

    QString message = regained < 0
        ? QString("%1 has no hit dice left to spend.").arg(name)
        : QString("%1 spends a d%2 hit die and regains %3 HP (now %4/%5). Hit dice left: %6/%7.")
              .arg(name)
              .arg(hit_die)
              .arg(regained)
              .arg(character.current_hp)
              .arg(character.max_hp)
              .arg(dnd::hit_dice_remaining(character))
              .arg(character.level);
    if (slotsRestored) message += " Pact spell slots restored.";
    if (featuresRestored) message += " Short-rest class features (like Second Wind) are ready again.";
    if (!recoveredSlots.isEmpty()) {
        message += QString(" Arcane Recovery restores %1 spell slot(s).").arg(recoveredSlots.join(", "));
    }
    QMessageBox::information(this, "Short Rest", message);
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
    auto characters = repository_.get_characters();
    std::vector<dnd::Character> living;
    for (const auto& character : characters) {
        if (!character.dead) living.push_back(character);
    }
    if (living.empty() && !characters.empty()) {
        QMessageBox::information(this, action,
            "Every character is dead. Only revival magic can bring them back, and that isn't available yet.");
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
    auto characters = charactersWhoCanFight("Solo Adventure");
    if (characters.empty() && !repository_.get_characters().empty()) return;

    std::vector<int> partyLevels;
    for (const auto& character : characters) partyLevels.push_back(character.level);
    auto monsters = repository_.get_monsters();
    AdventurePickerDialog picker(partyLevels, monsters, this);
    if (picker.exec() != QDialog::Accepted) return;
    dnd::AdventureTemplate adventure = picker.selectedAdventure().adventure;  // scaled for the party

    CombatEncounterDialog dialog(repository_, characters, monsters, this, &adventure);
    dialog.exec();
    reloadCharactersForCurrentPlayer();
}
