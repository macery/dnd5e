#pragma once

#include <QDialog>
#include <array>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_class.hpp"
#include "dnd/rules.hpp"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QComboBox;

class AddCharacterDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddCharacterDialog(const std::vector<dnd::CharacterClass>& classes, QWidget* parent = nullptr);

    // player_id is left at 0; the caller (MainWindow) fills it in.
    dnd::Character character() const;

private:
    QWidget* buildBasicsTab(const std::vector<dnd::CharacterClass>& classes);
    QWidget* buildAbilityScoresTab();
    QWidget* buildProficienciesTab();

    QLineEdit* nameEdit_ = nullptr;
    QComboBox* classCombo_ = nullptr;
    QSpinBox* raceSpin_ = nullptr;
    QSpinBox* alignmentSpin_ = nullptr;
    QSpinBox* levelSpin_ = nullptr;
    QSpinBox* currentHpSpin_ = nullptr;
    QSpinBox* maxHpSpin_ = nullptr;
    QSpinBox* armorClassSpin_ = nullptr;

    std::array<QSpinBox*, dnd::kAbilityCount> abilityScoreSpins_{};
    std::array<QCheckBox*, dnd::kAbilityCount> saveProficiencyBoxes_{};
    std::array<QCheckBox*, dnd::kSkillCount> skillProficiencyBoxes_{};
};
