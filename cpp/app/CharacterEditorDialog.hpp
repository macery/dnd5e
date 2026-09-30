#pragma once

#include <QDialog>
#include <array>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/rules.hpp"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QComboBox;

// Used for both adding a new character and editing an existing one. Pass
// `existing` to pre-populate every field from it and title the dialog
// "Edit Character"; pass nullptr for "Add Character" with defaults. Either
// way, character() just reflects the current field values -- the caller
// decides whether to add_character or update_character, and re-attaches
// character_id/player_id, which this dialog never touches.
class CharacterEditorDialog : public QDialog {
    Q_OBJECT

public:
    CharacterEditorDialog(const std::vector<dnd::LookupEntry>& classes, const std::vector<dnd::LookupEntry>& races,
                          const std::vector<dnd::LookupEntry>& alignments,
                          const dnd::Character* existing = nullptr, QWidget* parent = nullptr);

    dnd::Character character() const;

private:
    QWidget* buildBasicsTab(const std::vector<dnd::LookupEntry>& classes, const std::vector<dnd::LookupEntry>& races,
                            const std::vector<dnd::LookupEntry>& alignments);
    QWidget* buildAbilityScoresTab();
    QWidget* buildProficienciesTab();
    void populateFrom(const dnd::Character& existing);
    void refreshFightingStyle();

    QLineEdit* nameEdit_ = nullptr;
    QComboBox* classCombo_ = nullptr;
    QComboBox* raceCombo_ = nullptr;
    QComboBox* alignmentCombo_ = nullptr;
    QComboBox* fightingStyleCombo_ = nullptr;  // enabled for Fighters only
    QSpinBox* levelSpin_ = nullptr;
    QSpinBox* currentHpSpin_ = nullptr;
    QSpinBox* maxHpSpin_ = nullptr;

    std::array<QSpinBox*, dnd::kAbilityCount> abilityScoreSpins_{};
    std::array<QCheckBox*, dnd::kAbilityCount> saveProficiencyBoxes_{};
    std::array<QCheckBox*, dnd::kSkillCount> skillProficiencyBoxes_{};
};
