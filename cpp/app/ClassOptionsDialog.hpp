#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/class_options.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

class QLabel;
class QListWidget;

// The class choices a character has due (docs/class-features-1-5.md):
// Metamagic, Eldritch Invocations, Pact Boon, Pact of the Tome cantrips,
// Expertise, and a Lore Bard's bonus skills -- one checklist each. Accepting
// checks every count and saves the character.
class ClassOptionsDialog : public QDialog {
    Q_OBJECT

public:
    ClassOptionsDialog(dnd::Repository& repository, dnd::Character character,
                       const std::vector<dnd::LookupEntry>& classes, QWidget* parent = nullptr);

    // Spell names the choices need (cantrips for Pact of the Tome, and
    // whether the warlock knows Eldritch Blast).
    static dnd::SpellContext spellContext(const dnd::Repository& repository, const dnd::Character& character);

private:
    void choose();

    dnd::Repository& repository_;
    dnd::Character character_;
    std::string className_;
    dnd::SpellContext spells_;
    std::vector<dnd::ClassChoice> choices_;
    std::vector<QListWidget*> lists_;  // parallel to choices_
    QLabel* errorLabel_ = nullptr;
};
