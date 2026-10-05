#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dnd/damage.hpp"
#include "dnd/effects.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// One attack in a stat block (MonsterAttacks). An attack can add extra
// dice of another damage type (Hell Hound: + 2d6 fire) and a save against a
// condition on a hit (Wolf: DC 11 STR or prone).
struct MonsterAttack {
    std::string name;
    int attack_bonus = 0;
    int damage_dice_count = 1;
    int damage_dice_sides = 4;
    int damage_bonus = 0;
    std::string damage_type;  // "piercing", ...
    bool ranged = false;
    int extra_dice_count = 0;  // 0 = no extra damage
    int extra_dice_sides = 0;
    std::string extra_damage_type;
    Ability on_hit_save_ability = Ability::Strength;
    int on_hit_save_dc = 0;  // 0 = no on-hit effect
    Condition on_hit_condition = Condition::Prone;
    int on_hit_rounds = 0;            // how long the condition lasts; 0 = until removed (prone: until it stands)
    bool on_hit_repeat_save = false;  // the target repeats the save at the end of each of its turns
    int count = 1;  // made this many times per turn (Multiattack)
    // Monsters with a preferred attack use only those (archers, rock-throwers;
    // docs/content-5-10.md) -- there's no map to make them close in.
    bool preferred = false;
    // Life Drain (Wight, Wraith): on a hit, a CON save at this DC or the
    // target's max HP drops by the damage until a long rest. 0 = none.
    int life_drain_dc = 0;
};

// A recharging area attack (a dragon's breath): used when ready, it comes
// back at the start of the monster's turn on a d6 of `recharge` or more.
struct MonsterBreath {
    std::string name;             // "Cold Breath"
    int dice_count = 0;           // 0 for no damage (Petrifying Breath)
    int dice_sides = 8;
    std::string damage_type;
    Ability save_ability = Ability::Constitution;
    int save_dc = 10;
    int recharge = 5;
    int max_targets = 4;          // a cone or a line, without a map
    bool petrifying = false;      // Gorgon: restrained, then petrified on a second failure
};

// A spellcaster's magic (Cult Fanatic, Mage): seeded spells by name, slots
// per day by level, and its own save DC and attack bonus.
struct MonsterSpellcasting {
    int caster_level = 1;
    int save_dc = 10;
    int attack_bonus = 2;
    int ability_modifier = 0;
    std::vector<int> spell_slots;     // per day; index 0 = 1st level (not `slots`: a Qt macro)
    std::vector<std::string> spells;  // names of seeded spells
};

// A seeded SRD 5.1 stat block (docs/monsters-and-encounters.md).
// Deliberately excludes skills/saves/proficiency (5e monsters' bonuses are
// hand-picked exceptions, not a PC-style formula) and fields nothing uses
// yet (speed, size).
struct Monster {
    int monster_id = 0;
    std::string name;
    int armor_class = 10;
    int hit_points = 1;
    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;
    double challenge_rating = 0.0;

    std::string creature_type;  // "Humanoid", "Undead", "Beast", "Fiend", ...
    DamageTraits damage;
    std::vector<Condition> condition_immunities;
    // Multiattack, summarized for the UI ("A bite and two claw attacks").
    // With one, every attack is made (each `count` times); without, only the
    // first.
    std::string multiattack;
    std::vector<MonsterAttack> attacks;  // in order; at least one for every seeded monster
    // Traits the engine applies: "PackTactics", "MartialAdvantage",
    // "UndeadFortitude", "Regeneration".
    std::vector<std::string> traits;
    std::optional<MonsterBreath> breath;
    std::optional<MonsterSpellcasting> spellcasting;
};

}  // namespace dnd
