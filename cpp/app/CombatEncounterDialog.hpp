#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/combat.hpp"
#include "dnd/dice.hpp"
#include "dnd/monster.hpp"
#include "dnd/repository.hpp"
#include "dnd/weapon.hpp"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// The actual combat engine: an encounter of characters and (possibly
// multiple copies of) monsters, with initiative and attack resolution.
// Character HP changes persist via Repository; monster HP is in-memory
// only, consistent with monsters being unpersisted templates.
class CombatEncounterDialog : public QDialog {
    Q_OBJECT

public:
    CombatEncounterDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                           std::vector<dnd::Monster> monsters, std::vector<dnd::Weapon> weapons,
                           QWidget* parent = nullptr);

private slots:
    void onAddMonsterClicked();
    void onRollInitiativeClicked();
    void onAttackerChanged(int index);
    void onRollAttackClicked();

private:
    struct Participant {
        QString name;
        bool is_character = false;
        int character_id = -1;  // valid if is_character
        int armor_class = 10;
        int current_hp = 0;
        int max_hp = 0;
        int initiative = 0;
        int dex_modifier = 0;
        int attack_bonus = 0;
        int damage_dice_count = 1;
        int damage_dice_sides = 4;
        int damage_bonus = 0;
    };

    void addCharacterParticipants(const std::vector<dnd::Weapon>& weapons);
    void refreshParticipantTable();
    void refreshAttackCombos();
    void checkForVictoryOrDefeat();
    void log(const QString& line);

    dnd::Repository& repository_;
    std::vector<dnd::Character> characters_;
    std::vector<dnd::Monster> monsters_;
    std::vector<Participant> participants_;
    bool encounterOver_ = false;

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;

    QLabel* statusLabel_ = nullptr;
    QTableWidget* participantTable_ = nullptr;
    QComboBox* monsterTypeCombo_ = nullptr;
    QSpinBox* monsterCountSpin_ = nullptr;
    QPushButton* addMonsterButton_ = nullptr;
    QPushButton* rollInitiativeButton_ = nullptr;

    QComboBox* attackerCombo_ = nullptr;
    QComboBox* targetCombo_ = nullptr;
    QSpinBox* attackBonusSpin_ = nullptr;
    QSpinBox* damageDiceCountSpin_ = nullptr;
    QSpinBox* damageDiceSidesSpin_ = nullptr;
    QSpinBox* damageBonusSpin_ = nullptr;
    QPushButton* rollAttackButton_ = nullptr;

    QPlainTextEdit* logView_ = nullptr;
};
