#pragma once

#include <functional>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/dice.hpp"
#include "dnd/item.hpp"
#include "dnd/repository.hpp"
#include "dnd/rules.hpp"
#include "dnd/spell.hpp"

namespace dnd {

// Magic items (2014 DMG ch. 7; docs/magic-items.md): what an item's magic
// adds while it works, attunement, and spell scrolls. Weapon and armor
// bonuses are applied by weapon_attack and calculate_armor_class
// (dnd/equipment.hpp), which call these.

// Whether the item's magic works: weapons, armor, and shields while
// equipped (and attuned, if they require it); wondrous items while attuned
// (attuned counts as worn).
bool item_active(const InventoryEntry& entry);

// --- What active items add -----------------------------------------------------

// The character with STR and CON raised by active items (Gauntlets of Ogre
// Power, Amulet of Health) when that's higher than their own. HP is left
// alone: see item_hp_bonus.
Character with_item_scores(Character character, const std::vector<InventoryEntry>& inventory);

// The max HP an active Amulet of Health adds: level × (the CON modifier it
// gives - the character's own), never negative. `character` is the stored
// one (its own CON).
int item_hp_bonus(const Character& character, const std::vector<InventoryEntry>& inventory);

// Runs `change` (a level up) on the character with item HP taken off, then
// puts back the item HP for the result, so an Amulet of Health's bonus
// always matches the current level and the character's own CON.
void without_item_hp(Character& character, const std::vector<InventoryEntry>& inventory,
                     const std::function<void(Character&)>& change);

// Added to every saving throw (Cloak and Ring of Protection).
int item_save_bonus(const std::vector<InventoryEntry>& inventory);

// A saving throw bonus with items: raised scores plus item_save_bonus.
int saving_throw_bonus(const Character& character, Ability ability, const std::vector<InventoryEntry>& inventory);

// --- Attunement ------------------------------------------------------------------

constexpr int kMaxAttunedItems = 3;

int attuned_count(const std::vector<InventoryEntry>& inventory);

// Why `item_id` can't be attuned to now, or "" if it can: it must be carried,
// require attunement, not be attuned already, and the character must have
// fewer than three attuned items.
std::string attunement_refusal(const std::vector<InventoryEntry>& inventory, int item_id);

struct AttunementResult {
    bool ok = false;
    std::string message;  // a line for the player either way
};

// Attunes (after a short rest spent on it) or ends attunement, in one
// transaction. An Amulet of Health adds or removes its HP: attuning raises
// max and current HP; ending lowers max HP and caps current HP.
AttunementResult attune(Repository& repo, int character_id, int item_id);
AttunementResult end_attunement(Repository& repo, int character_id, int item_id);

// --- Spell scrolls -----------------------------------------------------------------

// The DMG's Spell Scroll table: save DC and attack bonus by spell level
// (13/+5 through 2nd, 15/+7 for 3rd-4th, 17/+9, 18/+10, 19/+11 for 9th).
int scroll_save_dc(int spell_level);
int scroll_attack_bonus(int spell_level);

// Whether a reader can use a scroll of `spell`.
struct ScrollReading {
    bool readable = false;
    std::string reason;    // why not, when !readable
    bool needs_check = false;  // above the reader's highest slot level
    int check_dc = 0;          // 10 + the spell's level
    int save_dc = 0;
    int attack_bonus = 0;
};

// Only classes with the spell on their list can read it. A spell above the
// reader's highest spell slot level (any leveled spell, for a class without
// slots at this level) needs a check with their spellcasting ability, DC 10
// + the spell's level; cantrips never do.
ScrollReading read_scroll(const Character& character, const ClassInfo& class_info, const Spell& spell);

// --- Reading scrolls outside combat -----------------------------------------------

// What reading a scroll between fights did.
struct ScrollUse {
    bool ok = false;           // the scroll was read (even if the attempt failed); false = nothing changed
    std::string message;       // lines for the player, newline-separated
};

// Whether a scroll can be read outside combat at all: only healing spells
// have anything to do away from a fight. Returns "" if it can, else why not.
std::string scroll_outside_combat_refusal(const Spell& spell);

// Reads `scroll_item_id` from the reader's pack on up to
// spell_max_targets characters, with the scroll's own level and numbers
// (read_scroll above, including the spellcasting check). Refused, with
// nothing changed, when the reader or a target is dead, nobody needs the
// healing, or the reader can't cast. The scroll is used up either way once
// it's read. Saves the healed characters and the pack in one transaction.
ScrollUse read_scroll_outside_combat(Repository& repo, const Dice& dice, int reader_id, int scroll_item_id,
                                     const std::vector<int>& target_ids);

}  // namespace dnd
