#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/spell.hpp"
#include "dnd/item.hpp"

class QLabel;
class QComboBox;

// Read-only view of a character's computed stats: ability modifiers,
// saving throws, skills, HP/AC, proficiency bonus, passive perception, and
// initiative -- all derived via dnd::rules, not stored directly -- plus, for
// casters, spellcasting stats, slots, and known spells. Each
// saving throw and skill can also be rolled directly (d20 + its bonus),
// under whichever roll mode (Normal/Advantage/Disadvantage) is selected.
class CharacterSheetDialog : public QDialog {
    Q_OBJECT

public:
    CharacterSheetDialog(const dnd::Character& character, const std::vector<dnd::LookupEntry>& classes,
                          const std::vector<dnd::LookupEntry>& races, const std::vector<dnd::LookupEntry>& alignments,
                          const std::vector<dnd::InventoryEntry>& inventory, const std::vector<dnd::Spell>& spells,
                          QWidget* parent = nullptr);

private:
    void rollAndShow(const QString& label, int bonus);

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;
    QLabel* lastRollLabel_ = nullptr;
    QComboBox* rollModeCombo_ = nullptr;
};
