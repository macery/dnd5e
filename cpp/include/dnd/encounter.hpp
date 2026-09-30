#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/effects.hpp"
#include "dnd/item.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/monster.hpp"
#include "dnd/spell.hpp"
#include "dnd/weapon.hpp"

namespace dnd {

// One side's fighter in an Encounter: a character (backed by a Character the
// encounter holds a copy of) or one copy of a monster stat block.
struct Combatant {
    std::string name;
    bool is_character = false;
    int character_id = -1;  // valid if is_character
    int armor_class = 10;
    int current_hp = 0;
    int max_hp = 0;
    int initiative = 0;
    int dex_modifier = 0;
    int attack_bonus = 0;
    int damage_dice_count = 1;
    int damage_dice_sides = 4;
    int damage_bonus = 0;
    bool melee_attack = true;  // the weapon/attack above; ranged changes how Prone applies
    int xp = 0;  // monsters only: awarded to the party on victory
    // Monsters only: STR..CHA, for saving throws. (Characters use their sheet.)
    std::array<int, 6> ability_scores{10, 10, 10, 10, 10, 10};

    // Characters at 0 HP: dying (rolling death saves), stable, or dead.
    // Monsters simply die at 0 HP.
    DeathSaveState death_saves;
    // The concentration spell this combatant is maintaining; empty if none.
    std::string concentration;
};

// Something the encounter did that the caller may need to show or persist.
// The encounter never touches the database itself.
struct EncounterEvent {
    enum class Kind {
        Log,               // `text` is a line for the combat log
        CharacterChanged,  // character(character_id) changed (HP, slots, XP, level): save it
        ItemUsed,          // character_id used `quantity` of item_id: remove from inventory
        ItemGained,        // character_id gained `quantity` of item_id: add to inventory
    };
    Kind kind = Kind::Log;
    std::string text;
    int character_id = -1;
    int item_id = 0;
    int quantity = 0;
};

enum class EncounterStatus { InProgress, Victory, Defeat };

// An item handed out on victory.
struct LootGrant {
    Item item;
    int quantity = 1;
};

// The combat engine: characters and (possibly several copies of) monsters
// fighting in initiative order. Monster turns resolve automatically; the
// caller acts for characters through the action methods. Combatant indices
// (into combatants()) are stable for the encounter's lifetime.
//
// Every action and turn change appends EncounterEvents; the caller drains
// them with take_events() to update its log and persist changes.
class Encounter {
public:
    // `dice` must outlive the encounter. `classes` resolves each character's
    // class (hit die, spellcasting); `weapons` their equipped weapon.
    Encounter(const Dice& dice, std::vector<LookupEntry> classes, std::vector<Weapon> weapons);

    // --- Setup ---------------------------------------------------------------
    // `known_spells` and `inventory` are the character's at the start of the
    // fight; the encounter keeps its own copies up to date as they're used.
    void add_character(const Character& character, std::vector<Spell> known_spells,
                       std::vector<InventoryEntry> inventory);
    // Adds `count` copies (numbered "#1", "#2", ... when count > 1).
    void add_monsters(const Monster& monster, int count);
    // Shown after "Victory!" and handed out on victory, for adventures.
    void set_victory_reward(std::string victory_text, std::vector<LootGrant> loot);

    // --- Turns ---------------------------------------------------------------
    // (Re)rolls initiative for everyone and runs monster turns until a living
    // character's turn or the end of the fight.
    void roll_initiative();
    // Ends the current turn (acted or not) and runs monster turns as above.
    void next_turn();

    // --- State ---------------------------------------------------------------
    const std::vector<Combatant>& combatants() const { return combatants_; }
    const Combatant* current() const;  // nullptr before initiative or when the fight is over
    int current_index() const;         // index into combatants(), or -1
    // True while it's a living character's turn and they haven't acted yet.
    bool awaiting_action() const;
    bool started() const { return !turn_order_.empty(); }
    EncounterStatus status() const { return status_; }
    // "Victory! All enemies defeated. <victory text>" / "Defeat! All characters
    // have fallen." once the fight ends; empty before.
    const std::string& result_message() const { return result_message_; }

    const Character* character(int character_id) const;

    // Every effect currently in play (conditions, Dodge, spell effects).
    const std::vector<ActiveEffect>& effects() const { return effects_; }
    // Adds an effect (its id is assigned here) and returns the id. For spells
    // and monster abilities, and for tests.
    int add_effect(ActiveEffect effect);
    void remove_effect(int effect_id);
    // `source` starts concentrating on `label`, ending any earlier
    // concentration. Effects added with concentration = true and this source
    // end when the concentration does.
    void start_concentration(int source, std::string label);
    void end_concentration(int source, const std::string& reason);

    // Base AC plus effect bonuses (Shield of Faith).
    int armor_class(int index) const;
    // A short status for the UI: "Up", "Up (Dodging, Prone)", "Dying (✓1 ✗2)",
    // "Stable", "Dead", or "Down" for a defeated monster.
    std::string status_text(int index) const;
    // Everyone on the other side from the current combatant who's still up.
    std::vector<int> attack_targets() const;
    // The current character's healing items with quantities.
    std::vector<InventoryEntry> healing_items() const;
    // The current character's known spells they have a slot for (cantrips always).
    std::vector<Spell> castable_spells() const;
    // Slot levels the current character could cast `spell` with (empty for cantrips).
    std::vector<int> spell_slot_options(const Spell& spell) const;
    // Heals: every character (including anyone down). Otherwise: living monsters.
    std::vector<int> spell_target_candidates(const Spell& spell) const;

    // --- Actions (the current character's one action for the turn) -----------
    // Each returns false -- leaving the action unused, with a log line saying
    // why where useful -- if it isn't a character's turn, they already acted,
    // or the choice isn't valid.
    //
    // A weapon attack with explicit numbers (the UI lets you override them;
    // defaults are the combatant's attack_bonus / damage_* fields).
    bool attack(int target, int attack_bonus, int damage_dice_count, int damage_dice_sides, int damage_bonus);
    bool dash();
    bool dodge();
    bool drink_potion(int item_id);
    // `slot_level` is 0 for cantrips; `targets` are combatant indices (ignored
    // for Utility spells).
    bool cast_spell(int spell_id, int slot_level, const std::vector<int>& targets);

    std::vector<EncounterEvent> take_events();

private:
    // The single paths every hit and every heal go through. apply_damage
    // handles dropping to 0 (dying, massive damage, monster death), damage
    // while dying, concentration checks, and charm-breaking; both return the
    // extra log lines to print after the line describing the hit or heal.
    std::vector<std::string> apply_damage(int target, int amount, bool critical, int source);
    std::vector<std::string> apply_healing(int target, int amount, int& healed);

    // Resolves a weapon attack with every effect applied, and logs it.
    void weapon_attack(int attacker, int target, int attack_bonus, int damage_dice_count, int damage_dice_sides,
                       int damage_bonus, bool melee);
    // d20 + the target's save bonus + effects, vs `dc`. Logs nothing.
    bool roll_save(int target, Ability ability, int dc, std::string& detail);
    int save_bonus(int index, Ability ability) const;
    std::vector<bool> alive_flags() const;
    bool is_conscious(int index) const;
    // Charmed by `target`: can't attack it.
    bool charmed_by(int attacker, int target) const;
    void use_effects(const std::vector<int>& effect_ids);

    // Effect bookkeeping at turn boundaries (durations, standing up, saves to end).
    void start_turn(int index);
    void end_turn(int index);
    // Rolls a dying character's death save at the start of their turn.
    // Returns true if they're back up and can act this turn.
    bool resolve_death_save(int index);
    // Marks a character dead (clearing its effects and concentration) and
    // returns the log lines to print.
    std::vector<std::string> mark_dead(int index, const std::string& how);
    // Ends `index`'s concentration and the effects that depended on it.
    // Returns the log line, or "" if it wasn't concentrating.
    std::string drop_concentration(int index, const std::string& reason);
    void fall_unconscious(int index);
    // Stabilizes everyone still dying (end of the fight).
    void stabilize_dying();

    Character* character_mut(int character_id);
    const Combatant* current_character() const;
    bool can_act() const;
    void advance_to_actionable_turn();
    void resolve_monster_turn(Combatant& monster);
    void check_for_victory_or_defeat();
    void award_victory_experience();
    void award_loot();
    // Copies a character combatant's HP onto its Character and reports it.
    void sync_character_hp(const Combatant& combatant);

    void log(std::string text);
    void changed(int character_id);

    const Dice& dice_;
    std::vector<LookupEntry> classes_;
    std::vector<Weapon> weapons_;

    std::vector<Character> characters_;
    std::map<int, std::vector<Spell>> known_spells_;
    std::map<int, std::vector<InventoryEntry>> inventories_;
    std::vector<Combatant> combatants_;
    std::vector<int> turn_order_;  // combatant indices, sorted by initiative
    int current_turn_ = -1;        // index into turn_order_; -1 = no active turn
    bool action_taken_ = false;
    EncounterStatus status_ = EncounterStatus::InProgress;
    std::string result_message_;
    std::string victory_text_;
    std::vector<LootGrant> loot_;
    std::vector<EncounterEvent> events_;
    std::vector<ActiveEffect> effects_;
    int next_effect_id_ = 1;
};

}  // namespace dnd
