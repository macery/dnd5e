#pragma once

#include <string>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/repository.hpp"

namespace dnd {

// Adding and preparing a character's spells in the database
// (docs/preparing-spells.md).

// Adds a spell to the character's list (a Wizard's spellbook), prepared if
// starts_prepared says so. Adding one they have already does nothing.
void add_known_spell(Repository& repo, const Character& character, const ClassInfo& class_info, int spell_id);

struct PrepareResult {
    bool ok = false;
    std::string message;
};

// Prepares or unprepares one of a Wizard's spellbook spells: only in the
// preparation window, never a cantrip, and never more than
// prepared_spell_count.
PrepareResult set_prepared(Repository& repo, const Character& character, const ClassInfo& class_info, int spell_id,
                           bool prepared);

}  // namespace dnd
