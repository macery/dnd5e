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
#include "dnd/spell.hpp"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

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
                           const dnd::AdventureTemplate* adventure = nullptr);

private slots:
    void onAddMonsterClicked();
    void onRollInitiativeClicked();
    void onNextTurnClicked();
    void onActionChanged(int index);
    void onTakeActionClicked();
    void onBonusActionClicked();
    void onActionSurgeClicked();

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
    void refreshSpellInfo();
    const dnd::Spell* selectedSpell() const;
    void log(const QString& line);

    dnd::Repository& repository_;
    std::vector<dnd::Monster> monsters_;
    std::vector<dnd::LookupEntry> classes_;
    // The current character's castable spells, parallel to spellCombo_.
    std::vector<dnd::Spell> castableSpells_;

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;
    dnd::Encounter encounter_;  // after dice_, which it references

    QLabel* statusLabel_ = nullptr;
    QLabel* objectiveLabel_ = nullptr;  // only created when launched from an adventure
    QLabel* difficultyLabel_ = nullptr;  // the fight's rating for the party
    QTableWidget* participantTable_ = nullptr;
    QComboBox* monsterTypeCombo_ = nullptr;
    QSpinBox* monsterCountSpin_ = nullptr;
    QPushButton* addMonsterButton_ = nullptr;
    QPushButton* rollInitiativeButton_ = nullptr;
    QPushButton* nextTurnButton_ = nullptr;

    QLabel* currentTurnLabel_ = nullptr;
    QLabel* moveLabel_ = nullptr;
    QLabel* bonusActionLabel_ = nullptr;
    QComboBox* actionCombo_ = nullptr;
    QComboBox* targetCombo_ = nullptr;
    QComboBox* potionCombo_ = nullptr;
    QComboBox* stabilizeCombo_ = nullptr;
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
    QCheckBox* recklessBox_ = nullptr;
    QComboBox* smiteCombo_ = nullptr;        // "No smite", then each slot level
    QComboBox* layOnHandsTargetCombo_ = nullptr;
    QSpinBox* layOnHandsSpin_ = nullptr;     // hit points to spend from the pool
    QLabel* layOnHandsInfoLabel_ = nullptr;
    // BonusTarget of each bonusCombo_ entry, by index.
    std::vector<dnd::Encounter::BonusTarget> bonusTargets_;
    QFormLayout* attackFormLayout_ = nullptr;

    QPlainTextEdit* logView_ = nullptr;
};
