#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/level_up.hpp"
#include "dnd/spell.hpp"

namespace dnd {

// Subclasses: the twelve in the SRD, one per class (2014 PHB ch. 3;
// docs/subclasses.md). Features through level 5 are automated where combat
// can apply them; later ones are listed by name.

// Subclass spells are either always prepared and free (Cleric domain,
// Paladin oath, Druid circle) or extra options to learn (Warlock patron).
enum class SubclassSpellMode { AlwaysPrepared, Expanded };

struct SubclassSpell {
    int class_level;           // gained at this class level
    const char* spell;         // a seeded spell's name
    const char* choice = "";   // only for this subclass choice (a Druid's land); "" for any
};

struct SubclassInfo {
    const char* name;          // "Champion", "Life Domain", ...
    const char* class_name;    // "Fighter"
    const char* summary;       // one line, for the chooser
    std::vector<ClassFeatureName> features;  // levels 1-20; `automated` marks what the app applies
    std::vector<SubclassSpell> spells;
    SubclassSpellMode spell_mode = SubclassSpellMode::AlwaysPrepared;
    const char* choice_label = "";           // "Dragon ancestor", "Land"; "" when there's no choice
    std::vector<const char*> choices;
};

const std::vector<SubclassInfo>& subclasses();
// nullptr for an unknown name.
const SubclassInfo* find_subclass(const std::string& name);
// The class's subclasses (one each, for now).
std::vector<const SubclassInfo*> subclasses_for(const std::string& class_name);
// The class level a subclass is chosen at: 1 (Cleric, Sorcerer, Warlock),
// 2 (Druid, Wizard), or 3; 0 for an unknown class.
int subclass_level(const std::string& class_name);
// The class's word for it: "Divine Domain", "Martial Archetype", ...
std::string subclass_kind(const std::string& class_name);
// A subclass is due: the character has reached its level and hasn't chosen.
bool subclass_due(const Character& character, const std::string& class_name);
// True if the character has this subclass at `min_level` or above.
bool has_subclass(const Character& character, const char* subclass, int min_level = 1);

// The subclass's features gained at exactly `level`.
std::vector<ClassFeatureName> subclass_features_at(const std::string& subclass, int level);

// Subclass spells the character has at their level (and choice).
std::vector<std::string> subclass_spell_names(const Character& character);
// AlwaysPrepared spells only (empty for Expanded subclasses).
std::vector<std::string> always_prepared_spell_names(const Character& character);
// Whether `spell` is on the character's spell list: their class's, or their
// subclass's expanded list (a Fiend warlock's Fireball).
bool on_spell_list(const Spell& spell, const std::string& class_name, const Character& character);

// Why this subclass (and choice) can't be taken now, or "": it must be the
// class's, due, and a valid choice when the subclass has one.
std::string subclass_refusal(const Character& character, const std::string& class_name, const std::string& subclass,
                             const std::string& choice);
// Takes the subclass: sets it (and the choice) and applies what comes with
// it at once (Draconic Resilience: +1 max HP per sorcerer level). Throws
// std::invalid_argument when subclass_refusal says no.
void choose_subclass(Character& character, const std::string& class_name, const std::string& subclass,
                     const std::string& choice);

}  // namespace dnd
