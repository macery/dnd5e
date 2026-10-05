#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// Class choices beyond the subclass (docs/class-features-1-5.md), stored in
// Character::class_options: Metamagic (Sorcerer), Eldritch Invocations and
// Pact Boon (Warlock), Pact of the Tome cantrips, Expertise (Rogue, Bard),
// and a Lore Bard's bonus skills.

// --- Warlock ---------------------------------------------------------------------

struct InvocationInfo {
    const char* name;
    int min_level;            // warlock level
    const char* prerequisite; // "Eldritch Blast", "Pact of the Blade", ... or ""
    const char* description;
    bool automated;
};

const std::vector<InvocationInfo>& invocations();
const InvocationInfo* find_invocation(const std::string& name);
// Invocations known: 2 at level 2, 3 at 5, 4 at 7, 5 at 9, 6 at 12, 7 at 15, 8 at 18.
int invocations_known(int warlock_level);

constexpr const char* kPactBoons[] = {"Pact of the Blade", "Pact of the Chain", "Pact of the Tome"};
const char* pact_boon_description(const std::string& pact);

// Armor of Shadows: Mage Armor at will (unarmored AC 13 + DEX).
bool has_armor_of_shadows(const Character& character);
// Pact of the Blade: proficient with the melee weapon you wield.
bool has_pact_blade(const Character& character);
// Agonizing Blast: +CHA modifier to each Eldritch Blast beam.
bool has_agonizing_blast(const Character& character);
// Thirsting Blade (Pact of the Blade, level 5): two attacks per Attack action.
bool has_thirsting_blade(const Character& character);

// --- Expertise ---------------------------------------------------------------------

// Skills with Expertise (double proficiency): Rogue 2 at 1 and 2 more at 6,
// Bard 2 at 3 and 2 more at 10.
int expertise_count(const std::string& class_name, int level);
bool has_expertise(const Character& character, Skill skill);

// --- What's due ------------------------------------------------------------------

struct ClassChoice {
    std::string key;            // "Metamagic", "Invocations", "PactBoon", "TomeCantrips", "Expertise", "BonusSkills"
    std::string label;          // "Eldritch Invocations"
    int count = 0;              // how many more to pick
    std::vector<std::string> options;  // what can be picked now
    std::vector<std::string> descriptions;  // parallel to options, for tooltips
};

// What a choice needs from the spell lists: every seeded cantrip (for Pact
// of the Tome) and the character's own spells (Agonizing Blast and Eldritch
// Spear need Eldritch Blast).
struct SpellContext {
    std::vector<std::string> cantrips;
    std::vector<std::string> known;
};

// The choices the character still has to make at their level (empty when none).
std::vector<ClassChoice> class_choices_due(const Character& character, const std::string& class_name,
                                           const SpellContext& spells = {});

// Adds `picks` to the choice: exactly `count` of the due choice's options,
// no repeats. Bonus skills become proficiencies too. Returns an error for the
// player, or "".
std::string apply_class_choice(Character& character, const std::string& class_name, const std::string& key,
                               const std::vector<std::string>& picks, const SpellContext& spells = {});

}  // namespace dnd
