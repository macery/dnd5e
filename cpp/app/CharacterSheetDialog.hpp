#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_class.hpp"
#include "dnd/dice.hpp"

class QLabel;

// Read-only view of a character's computed stats: ability modifiers,
// saving throws, skills, HP/AC, proficiency bonus, passive perception, and
// initiative -- all derived via dnd::rules, not stored directly. Each
// saving throw and skill can also be rolled directly (d20 + its bonus).
class CharacterSheetDialog : public QDialog {
    Q_OBJECT

public:
    CharacterSheetDialog(const dnd::Character& character, const std::vector<dnd::CharacterClass>& classes,
                          QWidget* parent = nullptr);

private:
    void rollAndShow(const QString& label, int bonus);

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;
    QLabel* lastRollLabel_ = nullptr;
};
