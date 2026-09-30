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
#include "dnd/turn.hpp"

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
    std::string attack_name;   // "Longsword", "Unarmed Strike", or the monster's attack
    int attacks_per_action = 1;  // Extra Attack (characters from level 5)
    // Characters only: wearing armor they aren't proficient with, which also
    // rules out spellcasting (PHB ch. 5).
    bool armor_not_proficient = false;
    int xp = 0;  // monsters only: awarded to the party on victory
    // Monsters only: STR..CHA, for saving throws. (Characters use their sheet.)
    std::array<int, 6> ability_scores{10, 10, 10, 10, 10, 10};
    // Monsters only: an effect their attack adds on a hit (see Monster).
    Ability on_hit_save_ability = Ability::Strength;
    int on_hit_save_dc = 0;
    Condition on_hit_condition = Condition::Prone;

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
    // `dice` must outlive the encounter. `classes` and `races` resolve each
    // character's class (hit die, spellcasting, proficiencies) and race
    // (weapon training).
    Encounter(const Dice& dice, std::vector<LookupEntry> classes, std::vector<LookupEntry> races);

    // --- Setup ---------------------------------------------------------------
    // `known_spells` and `inventory` are the character's at the start of the
    // fight; the encounter keeps its own copies up to date as they're used.
    // AC and the weapon attack come from the inventory's equipped items
    // (dnd/equipment.hpp).
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
    // True while it's a living character's turn and they still have an action
    // (or attacks left in an Attack action already started).
    bool awaiting_action() const;
    // True while it's a living character's turn and their bonus action is unused.
    bool bonus_action_available() const;
    // What the current character has left this turn.
    const TurnState& turn() const { return turn_; }
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
    // A short status for the UI: "Up", "Up (Dodging, Prone)", "Dying ✓○○ ✗✗○"
    // (death save successes / failures), "Stable", "Dead", or "Down" for a
    // defeated monster.
    std::string status_text(int index) const;
    // A multi-line, plain-language explanation of status_text: each effect with
    // its source, what it does, and how long it lasts, plus what dying,
    // stable, dead, and concentrating mean. One line per entry.
    std::vector<std::string> status_tooltip(int index) const;
    // Everyone on the other side from the current combatant who's still up.
    std::vector<int> attack_targets() const;
    // The current character's healing items with quantities.
    std::vector<InventoryEntry> healing_items() const;
    // The current character's known spells they could cast right now: a slot
    // is free (cantrips always), and the action or bonus action it needs is
    // available under the bonus-action spell rule (dnd/turn.hpp).
    std::vector<Spell> castable_spells() const;
    // Slot levels the current character could cast `spell` with (empty for cantrips).
    std::vector<int> spell_slot_options(const Spell& spell) const;
    // Who `spell` can target: heals, any character not dead (including anyone
    // down); buffs, conscious characters; Stabilize, dying characters;
    // otherwise living monsters. Empty for spells that target nobody.
    std::vector<int> spell_target_candidates(const Spell& spell) const;
    // Dying characters the current character could try to stabilize.
    std::vector<int> stabilize_targets() const;
    // True if the current character carries a Healer's Kit (stabilizing is automatic).
    bool has_healers_kit() const;

    // --- Actions ------------------------------------------------------------
    // Each returns false -- using nothing up, with a log line saying why where
    // useful -- if it isn't a character's turn, what it needs (action or bonus
    // action) is already spent, or the choice isn't valid.
    //
    // A weapon attack with explicit numbers (the UI lets you override them;
    // defaults are the combatant's attack_bonus / damage_* fields). The first
    // attack takes the action; with Extra Attack, the rest of that Attack
    // action's attacks follow without one.
    bool attack(int target, int attack_bonus, int damage_dice_count, int damage_dice_sides, int damage_bonus);
    bool dash();
    bool dodge();
    bool drink_potion(int item_id);
    // Stabilize a dying ally: a DC 10 WIS (Medicine) check, or automatic with a
    // Healer's Kit (not used up; kit uses aren't tracked yet).
    bool stabilize(int target);
    // `slot_level` is 0 for cantrips; `targets` are combatant indices (ignored
    // for Utility spells). The spell's casting time decides whether it takes
    // the action or the bonus action.
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
    // Applies `spell`'s effects with this trigger from `caster` to `target`;
    // returns their labels for the log.
    std::vector<std::string> apply_spell_effects(const Spell& spell, SpellEffectTrigger trigger, int caster,
                                                 int target);

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
    // It's a conscious, able character's turn (whatever they have left).
    bool character_can_act() const;
    // ...and they have an action left. If it's their turn but the action is
    // spent, logs why (so a click on an action button isn't silently ignored).
    bool can_take_action();
    // Spends the action on something other than an attack (giving up any
    // Extra Attack attacks left over).
    void use_action();
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
    std::vector<LookupEntry> races_;

    std::vector<Character> characters_;
    std::map<int, std::vector<Spell>> known_spells_;
    std::map<int, std::vector<InventoryEntry>> inventories_;
    std::vector<Combatant> combatants_;
    std::vector<int> turn_order_;  // combatant indices, sorted by initiative
    int current_turn_ = -1;        // index into turn_order_; -1 = no active turn
    TurnState turn_;
    EncounterStatus status_ = EncounterStatus::InProgress;
    std::string result_message_;
    std::string victory_text_;
    std::vector<LootGrant> loot_;
    std::vector<EncounterEvent> events_;
    std::vector<ActiveEffect> effects_;
    int next_effect_id_ = 1;
};

}  // namespace dnd
