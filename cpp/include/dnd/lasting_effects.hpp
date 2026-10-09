#pragma once

#include <map>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/item.hpp"
#include "dnd/spell.hpp"

namespace dnd {

// Spell effects that outlast a fight (docs/lasting-effects.md). They're saved
// on the character (Character::lasting_effects) and end at its next long
// rest, which stands in for their 8 hours.
//  - Mage Armor: unarmored AC 13 + DEX. Putting on armor ends it.
//  - Aid: max and current HP +5 per slot level above 1st. The bonus is
//    already added to max_hp, like Life Drain's reduction is taken off.

inline const std::string kMageArmor = "Mage Armor";
inline const std::string kAid = "Aid";

bool has_lasting_effect(const Character& character, const std::string& name);
// The HP an Aid cast with this slot gives: 5 at 2nd level, +5 per level above.
int aid_amount(int slot_level);
// The character's current Aid bonus, 0 without one.
int aid_bonus(const Character& character);

// A one-line summary for the spell list: "until a long rest: AC 13 + DEX
// while unarmored (1 creature)".
std::string lasting_summary(const Spell& spell);

// Why `target` can't get this spell now, or empty if it can: the dead, Mage
// Armor on someone wearing armor, an Aid no stronger than the one they have.
std::string lasting_refusal(const Spell& spell, const Character& target, const std::vector<InventoryEntry>& inventory,
                            int slot_level);
// Applies the spell to `target` (check lasting_refusal first) and returns
// what changed, e.g. "Mage Armor until a long rest" or "Aid: +5 max HP".
// A stronger Aid replaces a weaker one; they don't stack.
std::string apply_lasting_spell(const Spell& spell, Character& target, int slot_level);

// Ends every lasting effect (a long rest), taking Aid's HP back off.
void end_lasting_effects(Character& character);
// Ends Mage Armor if the inventory has armor equipped. Returns true if it did.
bool end_mage_armor_if_armored(Character& character, const std::vector<InventoryEntry>& inventory);

// For the character sheet: "Mage Armor, Aid (+5 HP)"; empty if none.
std::string describe_lasting_effects(const Character& character);

// "Mage Armor=0;Aid=5" and back, for the LastingEffects column.
std::string format_lasting_effects(const std::map<std::string, int>& effects);
std::map<std::string, int> parse_lasting_effects(const std::string& text);

}  // namespace dnd
