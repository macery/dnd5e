#pragma once

#include <QDialog>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/encounter.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/monster.hpp"
#include "dnd/repository.hpp"
#include "dnd/scenes.hpp"
#include "dnd/spell.hpp"

class QCheckBox;
class QComboBox;
class GridForm;
class QGridLayout;
class QGroupBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QSpinBox;
class QTableWidget;

// One fight of an adventure: the scene (already scaled for the party), the
// window title ("The Goblin Warren — scene 2 of 3: The Wolf Pens"), and the
// text shown on victory (the adventure's ending, on its last scene).
struct AdventureFight {
    QString title;
    dnd::AdventureScene scene;
    std::string victory_text;
    dnd::Surprise surprise = dnd::Surprise::None;  // from an earlier scene (docs/scenes-and-npcs.md)
    std::vector<dnd::FightNpc> allies;            // NPCs fighting beside the party
    std::vector<dnd::FightNpc> foes;              // ...and NPCs who have turned on it
};

// The combat screen: a view over a dnd::Encounter, which holds all the rules
// (turn order, monster turns, actions, victory and defeat). This dialog loads
// the encounter's inputs from the Repository, turns the player's choices into
// Encounter actions, shows its log, and persists what its events report --
// character changes and items used or gained. Monsters are unpersisted
// templates, so their HP lives only in the encounter.
class CombatEncounterDialog : public QDialog {
    Q_OBJECT

public:
    CombatEncounterDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                           std::vector<dnd::Monster> monsters, QWidget* parent = nullptr,
                           const AdventureFight* fight = nullptr);

    // How the fight ended: InProgress if the window was closed mid-fight.
    dnd::EncounterStatus outcome() const { return encounter_.status(); }
    // NPCs (ids) who died in the fight: named foes killed, allies left down in a lost fight.
    const std::vector<int>& fallenNpcs() const { return encounter_.fallen_npcs(); }

    // Closing (or Continue) hands out undivided loot as the panel shows it.
    void done(int result) override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAddMonsterClicked();
    void onRollInitiativeClicked();
    void onNextTurnClicked();
    void onActionChanged(int index);
    void onTakeActionClicked();
    void onBonusActionClicked();
    void onActionSurgeClicked();
    void onChannelClicked();
    void onWildShapeClicked();

private:
    // Logs and persists everything the encounter reported since last time.
    void processEvents();
    // Resets the turn controls for whoever's turn it now is.
    void onTurnStarted();
    // Status line, participant table, and whether the action button is live.
    void refreshState();
    void refreshParticipantTable();
    void refreshTargetCombo();
    void refreshPotionCombo();
    void refreshStabilizeCombo();
    void refreshHelpCombo();
    // The current character's non-spell bonus actions (class features) and
    // the Action Surge button.
    void refreshBonusControls();
    void refreshBonusTargets();
    // Reckless Attack, Divine Smite, and Lay on Hands for the current
    // character (hidden when they don't have them).
    void refreshFeatureControls();
    void refreshLayOnHandsAmount();
    // After anything the current character does: lists and counts change.
    void afterCharacterActed();
    void updateAttackFieldsForCurrentTurn();
    void updateActionControlsVisibility();

    // Fills the spell controls for the current character's turn: castable
    // known spells, then (via onSpellChanged) slots and targets.
    void refreshSpellControls();
    void onSpellChanged();
    void refreshMetamagic();
    void refreshSpellInfo();
    const dnd::Spell* selectedSpell() const;
    void log(const QString& line);
    // The Divide the Loot panel: shown after a victory with loot to share,
    // one row per item (per unit for small stacks) with who takes it.
    void refreshLootPanel();
    void handOutLoot();

    dnd::Repository& repository_;
    std::vector<dnd::Monster> monsters_;
    std::vector<dnd::LookupEntry> classes_;
    // The current character's castable spells, parallel to spellCombo_.
    std::vector<dnd::Spell> castableSpells_;
    bool showingScrolls_ = false;  // the spell controls list scrolls (Read Scroll) rather than spells

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;
    dnd::Encounter encounter_;  // after dice_, which it references

    QLabel* statusLabel_ = nullptr;
    QLabel* objectiveLabel_ = nullptr;  // only created when launched from an adventure
    QLabel* difficultyLabel_ = nullptr;  // the fight's rating for the party
    dnd::Difficulty shownDifficulty_ = dnd::Difficulty::Trivial;  // what difficultyLabel_ is colored for
    QTableWidget* participantTable_ = nullptr;
    QComboBox* monsterTypeCombo_ = nullptr;
    QSpinBox* monsterCountSpin_ = nullptr;
    QPushButton* addMonsterButton_ = nullptr;
    QPushButton* rollInitiativeButton_ = nullptr;
    QPushButton* nextTurnButton_ = nullptr;
    QPushButton* continueButton_ = nullptr;  // adventures: shown once the fight is over
    QCheckBox* reactionsBox_ = nullptr;      // Automatic reactions
    QGroupBox* lootGroup_ = nullptr;
    QGridLayout* lootGrid_ = nullptr;
    struct LootRow {
        dnd::Item item;
        int quantity = 1;
        QComboBox* recipient = nullptr;
    };
    std::vector<LootRow> lootRows_;

    QLabel* currentTurnLabel_ = nullptr;
    QLabel* moveLabel_ = nullptr;
    QLabel* bonusActionLabel_ = nullptr;
    QComboBox* actionCombo_ = nullptr;
    QComboBox* targetCombo_ = nullptr;
    QComboBox* potionCombo_ = nullptr;
    QComboBox* stabilizeCombo_ = nullptr;
    QComboBox* helpCombo_ = nullptr;
    QLabel* stabilizeInfoLabel_ = nullptr;
    QComboBox* spellCombo_ = nullptr;
    QComboBox* spellSlotCombo_ = nullptr;
    QListWidget* spellTargetList_ = nullptr;
    QLabel* spellInfoLabel_ = nullptr;
    QSpinBox* attackBonusSpin_ = nullptr;
    QSpinBox* damageDiceCountSpin_ = nullptr;
    QSpinBox* damageDiceSidesSpin_ = nullptr;
    QSpinBox* damageBonusSpin_ = nullptr;
    QPushButton* takeActionButton_ = nullptr;
    QComboBox* bonusCombo_ = nullptr;
    QComboBox* bonusTargetCombo_ = nullptr;
    QPushButton* bonusButton_ = nullptr;
    QPushButton* actionSurgeButton_ = nullptr;
    QWidget* channelRow_ = nullptr;          // Channel Divinity: shown when the current character has a use
    QComboBox* channelCombo_ = nullptr;
    QPushButton* channelButton_ = nullptr;
    QWidget* wildShapeRow_ = nullptr;        // Wild Shape: shown when the current druid can transform
    QComboBox* wildShapeCombo_ = nullptr;
    QCheckBox* recklessBox_ = nullptr;
    QCheckBox* stunningBox_ = nullptr;     // Monk 5: Stunning Strike on the next hit
    QCheckBox* inspirationBox_ = nullptr;  // spend inspiration for advantage
    QComboBox* metamagicCombo_ = nullptr;  // Sorcerer 3: Metamagic for the spell being cast
    QComboBox* smiteCombo_ = nullptr;        // "No smite", then each slot level
    QComboBox* layOnHandsTargetCombo_ = nullptr;
    QSpinBox* layOnHandsSpin_ = nullptr;     // hit points to spend from the pool
    QLabel* layOnHandsInfoLabel_ = nullptr;
    // BonusTarget of each bonusCombo_ entry, by index.
    std::vector<dnd::Encounter::BonusTarget> bonusTargets_;
    GridForm* attackFormLayout_ = nullptr;  // not a QFormLayout: rows here are hidden and shown (see GridForm)

    QPlainTextEdit* logView_ = nullptr;
    QSplitter* mainSplitter_ = nullptr;  // the fight | "Your Turn"
};
