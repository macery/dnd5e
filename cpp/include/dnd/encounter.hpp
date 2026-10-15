#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/class_features.hpp"
#include "dnd/dice.hpp"
#include "dnd/effects.hpp"
#include "dnd/encounter_difficulty.hpp"
#include "dnd/equipment.hpp"
#include "dnd/adventure.hpp"
#include "dnd/item.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/magic_items.hpp"
#include "dnd/monster.hpp"
#include "dnd/spell.hpp"
#include "dnd/spellcasting.hpp"
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
    bool attack_uses_strength = true;       // for Rage's damage bonus
    bool attack_finesse_or_ranged = false;  // for Sneak Attack
    bool attack_martial_arts = false;       // an unarmed strike or monk weapon (Martial Arts)
    bool attack_great_weapon_fighting = false;  // Fighting Style: reroll 1s and 2s on damage dice
    bool attack_magical = false;                // the weapon counts as magical (WeaponAttack::magical)
    std::string attack_damage_type;             // "slashing", ...

    // Characters only: class features (dnd/class_features.hpp).
    std::string class_name;
    int level = 0;
    int sneak_attack_dice = 0;  // Rogue: d6s, once per turn
    bool wearing_heavy_armor = false;  // no raging in heavy armor
    std::optional<WeaponAttack> martial_arts_strike;  // Monk: the bonus-action unarmed strike
    std::optional<WeaponAttack> unarmed_strike;       // Monk: Flurry of Blows (with or without Martial Arts)
    bool rage_kept_up = false;  // attacked or took damage since its last turn (else Rage ends)
    // Characters only: wearing armor they aren't proficient with, which also
    // rules out spellcasting (PHB ch. 5).
    bool armor_not_proficient = false;
    // Characters only: died during this encounter, so Revivify ("within the
    // last minute") can still bring them back.
    bool died_this_fight = false;
    // Used its reaction since the start of its last turn (Shield, Uncanny
    // Dodge, Hellish Rebuke).
    bool reaction_used = false;
    // Subclasses (docs/subclasses.md), exhaustion, and temporary HP.
    std::string subclass;            // characters only
    int exhaustion = 0;              // characters only: 1+ disadvantage on initiative, 3+ on attacks and saves
    int temp_hp = 0;                 // lost before hit points
    bool frenzied = false;           // a Berserker's Frenzy: exhaustion when the rage ends
    bool frenzy_ready = false;       // Frenzy attacks start on the turn after the rage
    double challenge_rating = 0;     // monsters only (Destroy Undead)
    // Monsters only (docs/content-5-10.md): a recharging breath weapon, and
    // spellcasting with the slots it has left.
    std::optional<MonsterBreath> breath;
    bool breath_ready = true;
    std::optional<MonsterSpellcasting> spellcasting;
    int initiative_bonus = 0;        // characters only: Jack of All Trades
    // Wild Shape: the druid's own numbers while in beast form; empty otherwise.
    struct DruidForm {
        std::string beast;
        int armor_class = 10;
        int current_hp = 0;
        int max_hp = 0;
        int dex_modifier = 0;
        int attack_bonus = 0;
        int damage_dice_count = 1;
        int damage_dice_sides = 4;
        int damage_bonus = 0;
        bool melee_attack = true;
        std::string attack_name;
        std::string attack_damage_type;
        int attacks_per_action = 1;
    };
    std::optional<DruidForm> druid_form;
    // Characters only: the *raise dead* penalty (Character::revival_penalty),
    // taken off attack rolls, saving throws, and initiative.
    int roll_penalty = 0;
    int xp = 0;  // monsters only: awarded to the party on victory
    // An NPC fighting on the party's side (docs/scenes-and-npcs.md, phase 3):
    // a stat block run by the monster AI against the monsters. It earns and
    // gives no XP, takes no loot, and is down (not dying) at 0 HP; healing
    // brings it back up.
    bool ally = false;
    // The NPC this combatant is (an ally, or a named foe among the monsters); 0 for none.
    int npc_id = 0;
    // Monsters only: STR..CHA, for saving throws. (Characters use their sheet.)
    std::array<int, 6> ability_scores{10, 10, 10, 10, 10, 10};
    // Monsters only: the stat block's attacks, creature type, and traits
    // (dnd/monster.hpp). Characters have none (Rage adds resistances while
    // it lasts; see damage_traits()).
    std::vector<MonsterAttack> monster_attacks;
    std::string creature_type;
    DamageTraits damage_traits;
    std::vector<Condition> condition_immunities;
    std::string multiattack;           // the stat block's Multiattack summary; empty if none
    std::vector<std::string> traits;   // "PackTactics", "MartialAdvantage", "UndeadFortitude", "Regeneration"
    bool regeneration_blocked = false; // took fire or acid damage since its last turn (Regeneration)
    std::string race_name;             // characters only (a Ghoul's paralysis spares elves)

    // Characters at 0 HP: dying (rolling death saves), stable, or dead.
    // Monsters simply die at 0 HP.
    DeathSaveState death_saves;
    // The concentration spell this combatant is maintaining; empty if none.
    std::string concentration;
};

// The party's side: the characters and their NPC allies.
inline bool party_side(const Combatant& combatant) {
    return combatant.is_character || combatant.ally;
}

// Something the encounter did that the caller may need to show or persist.
// The encounter never touches the database itself.
struct EncounterEvent {
    enum class Kind {
        Log,               // `text` is a line for the combat log
        CharacterChanged,  // character(character_id) changed (HP, slots, XP, level): save it
        ItemUsed,          // character_id used `quantity` of item_id: remove from inventory
        ItemUseSpent,      // character_id spent one use of item_id (Repository::spend_item_use)
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

// Part of the loot going to one character: `quantity` of `item`.
struct LootShare {
    Item item;
    int quantity = 1;
    int character_id = 0;
};

// A character standing at the end of a fight, for dividing the loot.
struct LootRecipient {
    int character_id = 0;
    int current_hp = 0;
    int max_hp = 1;
};

// The suggested split of `loot` among `recipients` (in party order), one
// item at a time: healing potions to the most hurt first (lowest HP
// fraction; each one goes to the next most hurt), then everything else to
// whoever has received the fewest items so far (ties: party order). Shares
// for the same item and character are merged, in the loot's order.
std::vector<LootShare> suggest_loot_shares(const std::vector<LootGrant>& loot,
                                           const std::vector<LootRecipient>& recipients);

// Choices made with a character's attack (Encounter::attack).
struct AttackOptions {
    // Reckless Attack; ignored unless Encounter::can_attack_recklessly().
    bool reckless = false;
    // Spend inspiration (PHB ch. 4) for advantage on this attack. Ignored
    // unless the attacker has it.
    bool inspired = false;
    // Divine Smite: spend a slot of this level on a hit (0 = no smite). The
    // attack is refused if it isn't in Encounter::divine_smite_slots(). A
    // miss spends nothing.
    int divine_smite_slot = 0;
    // Stunning Strike (Monk 5): spend 1 ki on a hit; the target makes a CON
    // save or is stunned until the end of the monk's next turn. Ignored
    // unless Encounter::can_stunning_strike().
    bool stunning_strike = false;
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
    // An NPC who fights for the party with `monster`'s stat block, named
    // "Dagna Ironfoot (Veteran)".
    void add_ally(const Monster& monster, const std::string& name, int npc_id);
    // An NPC who has turned on the party, fighting among the monsters with
    // `monster`'s stat block: a monster in every way (XP included), but named.
    void add_named_foe(const Monster& monster, const std::string& name, int npc_id);
    // The NPCs (ids) who died in this fight, once it's over: named foes at 0
    // HP, and allies still down when the fight was lost.
    const std::vector<int>& fallen_npcs() const { return fallen_npcs_; }
    // Shown after "Victory!" and handed out on victory, for adventures.
    // `coins_cp` is split evenly among the surviving characters (dnd/currency.hpp).
    // Items go to the only character standing at once; with more, they wait
    // for divide_loot (see loot_pending).
    void set_victory_reward(std::string victory_text, std::vector<LootGrant> loot, int coins_cp = 0);

    // --- Loot ----------------------------------------------------------------
    // After a victory with two or more characters standing, the loot waits
    // here until it's divided ("Loot found: ..." is logged meanwhile).
    bool loot_pending() const { return !pending_loot_.empty(); }
    const std::vector<LootGrant>& pending_loot() const { return pending_loot_; }
    // The characters standing, in party order: who can take loot.
    std::vector<LootRecipient> loot_recipients() const;
    // suggest_loot_shares for the pending loot and loot_recipients().
    std::vector<LootShare> suggested_loot_shares() const;
    // Hands out the pending loot: logs "Loot! Clark takes: ..." per
    // character and reports ItemGained events. Refused (false, nothing
    // changes) unless the shares cover exactly the pending loot, item by
    // item, and go only to loot_recipients().
    bool divide_loot(const std::vector<LootShare>& shares);

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

    // Reactions (docs/reactions.md) happen on monsters' turns, so they're
    // automatic: Shield when it turns a hit into a miss, Uncanny Dodge on a
    // hit, Hellish Rebuke when damaged. On by default.
    void set_auto_reactions(bool on) { auto_reactions_ = on; }
    // Who's surprised when the fight starts (docs/scenes-and-npcs.md):
    // surprised combatants lose their first turn. Set before rolling initiative.
    void set_surprise(Surprise surprise) { surprise_ = surprise; }
    // The current character has inspiration to spend (see AttackOptions).
    bool has_inspiration() const;
    bool auto_reactions() const { return auto_reactions_; }
    EncounterStatus status() const { return status_; }
    // "Victory! All enemies defeated. <victory text>" / "Defeat! All characters
    // have fallen." once the fight ends; empty before.
    const std::string& result_message() const { return result_message_; }

    const Character* character(int character_id) const;
    // How hard this fight is for the characters in it (dead ones excluded),
    // counting every monster added (dnd/encounter_difficulty.hpp).
    EncounterRating difficulty() const;

    // Every effect currently in play (conditions, Dodge, spell effects).
    const std::vector<ActiveEffect>& effects() const { return effects_; }
    // Adds an effect (its id is assigned here) and returns the id. For spells
    // and monster abilities, and for tests. A condition the target is immune
    // to is refused: logged, and 0 is returned.
    int add_effect(ActiveEffect effect);
    void remove_effect(int effect_id);
    // `source` starts concentrating on `label`, ending any earlier
    // concentration. Effects added with concentration = true and this source
    // end when the concentration does.
    void start_concentration(int source, std::string label);
    void end_concentration(int source, const std::string& reason);

    // Base AC plus effect bonuses (Shield of Faith).
    int armor_class(int index) const;
    // The combatant's resistances, vulnerabilities, and immunities right now:
    // a monster's stat block, plus bludgeoning/piercing/slashing resistance
    // while raging.
    DamageTraits damage_traits(int index) const;
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

    // --- Class features -----------------------------------------------------
    enum class BonusAction {
        SecondWind,
        Rage,
        CunningDash,
        CunningDisengage,
        BardicInspiration,
        MartialArtsStrike,
        FlurryOfBlows,          // Monk, 1 ki: two unarmed strikes after the Attack action
        PatientDefense,         // Monk, 1 ki: Dodge
        StepOfTheWindDash,      // Monk, 1 ki: Dash (shown only)
        StepOfTheWindDisengage, // Monk, 1 ki: Disengage (shown only)
        RageWithFrenzy,         // Berserker: Rage, and Frenzy (exhaustion when it ends)
        FrenzyAttack,           // Berserker: one melee weapon attack while frenzied
        FastHandsPotion,        // Thief: drink a potion (the target is the potion's item id)
        CreateSpellSlot,        // Sorcerer: Font of Magic, sorcery points into a slot (the target is its level)
        RevertWildShape,        // Druid: back to druid form
    };
    enum class BonusTarget { None, Ally, Enemy, Potion, SlotLevel };
    struct BonusActionOption {
        BonusAction kind;
        std::string label;        // "Second Wind (1 left)"
        std::string description;  // for tooltips
        BonusTarget target = BonusTarget::None;
    };
    // The current character's non-spell bonus actions available right now.
    // (Bonus-action spells go through cast_spell.)
    std::vector<BonusActionOption> bonus_action_options() const;
    // Candidates for a BonusTarget: allies are other conscious characters;
    // enemies are living monsters.
    std::vector<int> bonus_action_targets(BonusTarget target) const;
    bool use_bonus_action(BonusAction kind, int target = -1);

    // Channel Divinity (Clerics from 2, Paladins from 3; docs/subclasses.md):
    // one action, one use, back on a short rest.
    enum class Channel { TurnUndead, PreserveLife, SacredWeapon, TurnTheUnholy };
    struct ChannelOption {
        Channel kind;
        std::string label;        // "Channel Divinity: Turn Undead (1 left)"
        std::string description;
    };
    std::vector<ChannelOption> channel_divinity_options() const;
    bool channel_divinity(Channel kind);
    // Fighter Action Surge: one more action this turn.
    bool can_action_surge() const;
    bool action_surge();
    // Barbarian Reckless Attack (level 2): decided on the first attack of the
    // turn (see AttackOptions).
    bool can_attack_recklessly() const;
    // Everything that shapes a combatant's saving throw right now: effects,
    // exhaustion, Danger Sense, ...
    RollModifiers save_modifiers(int index, Ability ability) const;
    // Wild Shape (Druid 2; docs/class-features-1-5.md). The encounter only
    // knows the beasts it's given (every seeded monster will do; it picks the
    // beasts).
    void set_wild_shape_beasts(std::vector<Monster> monsters);
    // Beasts the current druid can become now (CR limit, uses left, an action).
    std::vector<Monster> wild_shape_forms() const;
    // Become `beast` (an action and a use): its HP, AC, and attacks replace the druid's.
    bool wild_shape(const std::string& beast);
    // Back to druid form (a bonus action on the druid's turn, or at 0 HP).
    std::string revert_wild_shape(int index, const std::string& reason);
    // Monk Stunning Strike (level 5) with ki left (see AttackOptions).
    bool can_stunning_strike() const;
    // Paladin Divine Smite (level 2): slot levels the current character could
    // smite with on a melee hit; empty if none.
    std::vector<int> divine_smite_slots() const;
    // Paladin Lay on Hands: hit points left in the pool (0 without the
    // feature), who can be touched (any character not dead, self included),
    // and the action itself: restores `amount` (1..pool) hit points.
    int lay_on_hands_pool() const;
    std::vector<int> lay_on_hands_targets() const;
    bool lay_on_hands(int target, int amount);

    // --- Actions ------------------------------------------------------------
    // Each returns false -- using nothing up, with a log line saying why where
    // useful -- if it isn't a character's turn, what it needs (action or bonus
    // action) is already spent, or the choice isn't valid.
    //
    // A weapon attack with explicit numbers (the UI lets you override them;
    // defaults are the combatant's attack_bonus / damage_* fields). The first
    // attack takes the action; with Extra Attack, the rest of that Attack
    // action's attacks follow without one.
    // See AttackOptions for Reckless Attack and Divine Smite.
    bool attack(int target, int attack_bonus, int damage_dice_count, int damage_dice_sides, int damage_bonus,
                AttackOptions options = {});
    bool dash();
    bool dodge();
    bool drink_potion(int item_id);
    // Stabilize a dying ally: a DC 10 WIS (Medicine) check, or automatic with a
    // Healer's Kit (not used up; kit uses aren't tracked yet).
    bool stabilize(int target);
    // Help: another character (not the helper, not a dead or dying one) has
    // advantage on their next attack roll before the helper's next turn. The
    // PHB ties it to an enemy within 5 feet; with no map, any ally will do.
    std::vector<int> help_targets() const;
    bool help(int target);
    // `slot_level` is 0 for cantrips; `targets` are combatant indices (ignored
    // for Utility spells). The spell's casting time decides whether it takes
    // the action or the bonus action.
    bool cast_spell(int spell_id, int slot_level, const std::vector<int>& targets,
                    Metamagic metamagic = Metamagic::None);
    // Metamagic the current sorcerer could apply to `spell` at `slot_level`
    // (chosen options, enough sorcery points, and a spell it fits), with each
    // one's cost in points.
    std::vector<std::pair<Metamagic, int>> metamagic_options(const Spell& spell, int slot_level) const;

    // Spell scrolls (docs/magic-items.md). The encounter only knows the
    // spells it's given: every spell a scroll might cast.
    void set_scroll_spells(std::vector<Spell> spells);
    const Spell* scroll_spell(const Item& scroll) const;
    // A character's pack as the fight has left it (potions drunk, loot taken).
    const std::vector<InventoryEntry>* pack_of(int character_id) const { return inventory_of(character_id); }
    // The current character's scrolls of known spells.
    std::vector<InventoryEntry> scrolls() const;
    // Whether the current character can read it, and how (dnd/magic_items.hpp).
    std::optional<ScrollReading> scroll_reading(const Item& scroll) const;
    // Reads a scroll as the spell's casting time: the scroll's own save DC
    // and attack bonus, at its own level, with no slot spent. A spell above
    // the reader's highest slot needs a spellcasting check first; on a
    // failure the scroll is wasted. Either way it's used up (ItemUsed).
    // False, with nothing used, if it can't be read or the targets are wrong.
    bool read_scroll(int item_id, const std::vector<int>& targets);

    std::vector<EncounterEvent> take_events();

private:
    // The single paths every hit and every heal go through. apply_damage
    // handles dropping to 0 (dying, massive damage, monster death), damage
    // while dying, concentration checks, and charm-breaking; both return the
    // extra log lines to print after the line describing the hit or heal.
    // `damage_type` is "radiant" if any of the damage was (Undead Fortitude).
    std::vector<std::string> apply_damage(int target, int amount, bool critical, int source,
                                          const std::string& damage_type = "");
    std::vector<std::string> apply_healing(int target, int amount, int& healed);

    // Resolves a weapon attack with every effect and class feature applied
    // (Rage, Sneak Attack, Divine Favor, Reckless Attack, Great Weapon
    // Fighting, a monster's rider), and logs it. `smite_slot` > 0 spends a
    // slot of that level on a hit for Divine Smite.
    // `monster_attack` adds a stat block attack's extra damage and on-hit save.
    // Returns true on a hit.
    bool weapon_attack(int attacker, int target, const WeaponAttack& attack, int smite_slot = 0,
                       const MonsterAttack* monster_attack = nullptr);
    // `amount` of `type` damage to `target` after its resistances,
    // vulnerabilities, and immunities; appends " (halved: resistant to
    // slashing)"-style notes to `note`.
    // Fire or acid also stops a Troll's Regeneration on its next turn.
    // `magical` is false for a nonmagical weapon (or a monster's attack).
    int typed_damage(int target, int amount, const std::string& type, std::string& note, bool magical = true);
    bool has_trait(int index, const std::string& trait) const;
    // Another combatant on `index`'s side is up and not incapacitated (Pack
    // Tactics, Martial Advantage, Sneak Attack: with no map, "an ally within
    // 5 feet of the target").
    bool ally_fighting(int index) const;
    bool is_raging(int index) const;
    // Ends a rage early; returns the log line ("" if not raging).
    std::string end_rage(int index, const std::string& reason);
    const std::vector<InventoryEntry>* inventory_of(int character_id) const;
    // d20 + the target's save bonus + effects, vs `dc`. Logs nothing.
    bool roll_save(int target, Ability ability, int dc, std::string& detail);
    int save_bonus(int index, Ability ability) const;
    std::vector<bool> alive_flags() const;
    bool is_conscious(int index) const;
    // Charmed by `target`: can't attack it.
    bool charmed_by(int attacker, int target) const;
    void use_effects(const std::vector<int>& effect_ids);
    // Applies `spell`'s effects with this trigger from `caster` to `target`;
    // returns their labels for the log. `save_dc` is the caster's, for
    // effects the target can save against at the end of each turn.
    std::vector<std::string> apply_spell_effects(const Spell& spell, SpellEffectTrigger trigger, int caster,
                                                 int target, int save_dc = 0);
    // Revivify: brings a character who died in this fight back with 1 HP.
    std::string revive(int target);
    // A character's prepared spell by name with a slot to cast it, or nullptr.
    const Spell* reaction_spell(int index, const std::string& name) const;
    // Spends the lowest slot that can cast `spell`; returns its level (0: none).
    int spend_lowest_slot(int index, const Spell& spell);
    // Monster abilities (docs/content-5-10.md).
    void use_breath(int monster, const std::vector<int>& candidates);
    // A caster monster's turn: casts its best spell and returns true, or false
    // to attack instead.
    bool monster_cast(int monster, const std::vector<int>& candidates);
    // A seeded spell by name (from set_scroll_spells), or nullptr.
    const Spell* spell_named(const std::string& name) const;
    // Life Drain after a hit: a CON save or max HP drops by `damage`.
    void life_drain(int target, int dc, int damage);
    // Hellish Rebuke back at `attacker` after `index` took damage from it.
    void hellish_rebuke(int index, int attacker);
    // Cutting Words: a Lore bard subtracts an inspiration die from a monster's
    // attack total against `target`; returns the amount subtracted (0: none).
    int cutting_words(int target, int attack_total, int armor_class);
    // Gains (or loses) exhaustion levels; dies at 6.
    void add_exhaustion(int index, int levels, const std::string& why);
    // A frenzied Berserker whose rage is over pays for it.
    void settle_frenzy(int index);
    // Drinks a healing potion from the character's pack (Drink Potion, Fast Hands).
    bool drink(int index, int item_id);
    // Disadvantage from exhaustion on attack rolls and saves (level 3+).
    void exhaustion_mods(int index, RollModifiers& mods) const;
    // Class features on a saving throw: exhaustion, and a Barbarian's Danger
    // Sense (advantage on DEX saves unless blinded, deafened, or incapacitated).
    void save_feature_mods(int index, Ability ability, RollModifiers& mods) const;
    // Stunning Strike after a hit; logs the save.
    void stunning_strike(int monk, int target);

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
    // The target `chooser` goes for among `candidates` (dnd/tactics.hpp): a
    // weighted roll. When every weight is the same, it's a plain d(n).
    int pick_target(int chooser, const std::vector<int>& candidates);
    // `candidates` in the order `chooser` prefers them (ties keep their order).
    std::vector<int> by_preference(int chooser, std::vector<int> candidates) const;
    void check_for_victory_or_defeat();
    void award_victory_experience();
    void award_loot();
    // Logs why and returns false unless `targets` suit the spell.
    bool spell_targets_ok(const Spell& spell, int caster_level, int slot, const std::vector<int>& targets);
    // Everything after paying for a spell: the action, the rolls, effects,
    // and the log (`cast_line` first).
    void resolve_spell(int actor_index, const Spell& spell, const CasterStats& caster, int slot,
                       const std::vector<int>& targets, const std::string& cast_line);
    // Adds the shares to the inventories, logs them, and reports ItemGained.
    void hand_out(const std::vector<LootShare>& shares);
    void award_coins();
    // Copies a character combatant's HP onto its Character and reports it.
    void sync_character_hp(const Combatant& combatant);
    // Mage Armor or Aid on a character: saved on the Character so it lasts
    // past the fight, and the combatant's AC and HP follow. Returns the outcome.
    std::string apply_lasting(const Spell& spell, int slot, int target_index);

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
    std::vector<int> fallen_npcs_;  // npc ids (see fallen_npcs())
    std::string victory_text_;
    std::vector<LootGrant> loot_;
    std::vector<Spell> scroll_spells_;
    std::vector<Monster> beasts_;  // Wild Shape forms
    bool auto_reactions_ = true;
    Surprise surprise_ = Surprise::None;
    std::vector<int> surprised_;  // combatants still to lose their first turn
    bool spend_inspiration_ = false;  // the next weapon attack has advantage from inspiration
    int heightened_target_ = -1;  // Heightened Spell: this target has disadvantage on its save
    std::vector<std::string> pending_reaction_lines_;  // logged after the attack they answered
    std::vector<LootGrant> pending_loot_;  // won, waiting for divide_loot
    int coins_cp_ = 0;
    std::vector<EncounterEvent> events_;
    std::vector<ActiveEffect> effects_;
    int next_effect_id_ = 1;
};

}  // namespace dnd
