#pragma once

#include <QMainWindow>
#include <vector>

#include "dnd/adventure_run.hpp"
#include "dnd/dice.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QModelIndex;
class CharacterTableModel;
class DiceRollerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(dnd::Repository& repository, QWidget* parent = nullptr);

private slots:
    void onPlayerSelected(int index);
    void onAddPlayerClicked();
    void onEditPlayerClicked();
    void onDeletePlayerClicked();
    void onAddCharacterClicked();
    void onEditCharacterClicked();
    void onDeleteCharacterClicked();
    void onCharacterActivated(const QModelIndex& index);
    void onInventoryClicked();
    void onShopClicked();
    void onSpellbookClicked();
    void onLevelUpClicked();
    void onChooseSubclassClicked();
    // Opens the subclass chooser if the character has one due; true if one was chosen.
    bool offerSubclass(const dnd::Character& character);
    void onClassOptionsClicked();
    // Opens the class options dialog if the character has choices due.
    bool offerClassOptions(const dnd::Character& character);
    // The character fresh from the database (after a dialog saved it).
    dnd::Character reloaded(const dnd::Character& character) const;
    void onExportClicked();
    void onImportClicked();
    void onBackupClicked();
    void onDamageClicked();
    void onHealClicked();
    void onShortRestClicked();
    void onLongRestClicked();
    void onInitiativeTrackerClicked();
    void onMonstersClicked();
    void onCombatEncounterClicked();
    void onSoloAdventureClicked();
    void onCampaignSelected(int index);
    void onCampaignsClicked();
    void onNpcsClicked();

private:
    // Plays a multi-scene adventure from wherever `run` is, saving it after each step.
    void playAdventure(dnd::AdventureRun run);
    void reloadPlayers();
    void reloadLookupTables();
    // The campaign selector (and window title) from the database.
    void reloadCampaigns();
    // The player a new character or export is for: the filter's, or with
    // "All players", the selected character's or one the user picks. 0 if
    // none (cancelled, or no players).
    int playerForAction(const QString& action);
    void reloadCharactersForCurrentPlayer();
    void adjustCurrentCharacterHp(int delta);
    // The selected character, or nullptr (after telling the user to pick
    // one, titled `action`) if nothing is selected.
    const dnd::Character* selectedCharacter(const QString& action);
    int currentPlayerId() const;
    // Every character who can still fight (not dead). If that leaves nobody
    // but some characters exist, explains why (titled `action`) and returns
    // an empty list; the caller should then stop.
    std::vector<dnd::Character> charactersWhoCanFight(const QString& action);
    QString currentPlayerName() const;

    dnd::Repository& repository_;
    std::vector<dnd::LookupEntry> classes_;
    std::vector<dnd::LookupEntry> races_;
    std::vector<dnd::LookupEntry> alignments_;

    QComboBox* playerCombo_ = nullptr;
    QComboBox* campaignCombo_ = nullptr;
    QLineEdit* newPlayerName_ = nullptr;
    QPushButton* addPlayerButton_ = nullptr;
    QPushButton* editPlayerButton_ = nullptr;
    QPushButton* deletePlayerButton_ = nullptr;
    QTableView* characterTable_ = nullptr;
    QPushButton* addCharacterButton_ = nullptr;
    QPushButton* editCharacterButton_ = nullptr;
    QPushButton* deleteCharacterButton_ = nullptr;
    QPushButton* inventoryButton_ = nullptr;
    QPushButton* shopButton_ = nullptr;
    QPushButton* subclassButton_ = nullptr;
    QPushButton* classOptionsButton_ = nullptr;
    QPushButton* spellbookButton_ = nullptr;
    QPushButton* levelUpButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QPushButton* backupButton_ = nullptr;
    QSpinBox* hpAmountSpin_ = nullptr;
    QPushButton* damageButton_ = nullptr;
    QPushButton* healButton_ = nullptr;
    QPushButton* shortRestButton_ = nullptr;
    QPushButton* longRestButton_ = nullptr;
    QPushButton* initiativeTrackerButton_ = nullptr;
    QPushButton* monstersButton_ = nullptr;
    QPushButton* combatEncounterButton_ = nullptr;
    QPushButton* soloAdventureButton_ = nullptr;

    CharacterTableModel* characterModel_ = nullptr;
    DiceRollerWidget* diceRoller_ = nullptr;
};
