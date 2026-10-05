#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dnd {

struct Character {
    int character_id = 0;
    std::string character_name;
    int class_id = 0;
    int race = 0;
    int alignment = 0;
    int player_id = 0;
    // The campaign this character plays in (0 = none). Changed only with
    // Repository::set_character_campaign, never by update_character.
    int campaign_id = 0;
    int level = 1;

    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;

    int current_hp = 0;
    int max_hp = 0;
    // AC and the wielded weapon aren't stored: they come from equipped
    // inventory items (dnd/equipment.hpp).
    std::string background;  // a dnd::BackgroundInfo name; empty if none
    // Failed three death saves (or took massive damage). Stays dead until
    // revived by a spell -- see docs/effects-system.md, Decisions.
    bool dead = false;
    // Raised by *raise dead*: a penalty (4 at first) to attack rolls, saving
    // throws, and ability checks that drops by 1 each long rest (dnd/shop.hpp).
    int revival_penalty = 0;
    // Prepared casters may change their prepared spells: after a long rest,
    // until their next fight (docs/preparing-spells.md).
    bool may_prepare = true;
    // The subclass (dnd/subclass.hpp; "" until chosen) and its choice: a
    // Draconic sorcerer's dragon, a Land druid's land.
    std::string subclass;
    std::string subclass_choice;
    // Exhaustion levels, 0-6 (PHB appendix A); a long rest removes one.
    int exhaustion = 0;
    // Temporary hit points, lost first and gone after a long rest.
    int temp_hp = 0;
    // Life Drain (Wight, Wraith): max HP taken away until a long rest, already
    // subtracted from max_hp; the long rest gives it back.
    int max_hp_reduction = 0;
    // Class choices beyond the subclass (docs/class-features-1-5.md), by key:
    // "Metamagic", "Invocations", "PactBoon", "Expertise", "TomeCantrips",
    // "BonusSkills" -- each a list of names.
    std::map<std::string, std::vector<std::string>> class_options;
    // Class feature uses spent since they last came back, by feature key
    // ("SecondWind", "Rage", ...). Maximums come from class and level
    // (dnd/class_features.hpp); rests clear them.
    std::map<std::string, int> feature_uses_spent;
    // A Fighter's Fighting Style ("Defense", "Archery", ...); empty if none
    // (see dnd/class_features.hpp).
    std::string fighting_style;
    // Coins carried: cp, sp, ep, gp, pp (dnd/currency.hpp's Coin order).
    std::array<int, 5> coins{};

    int experience = 0;
    // Spent on short rests, partly recovered on a long rest; the total
    // available is one per level (see dnd/progression.hpp).
    int hit_dice_used = 0;
    // Spell slots spent since they were last restored, indexed by slot
    // level - 1. Totals come from class + level (see dnd/spellcasting.hpp).
    std::array<int, 9> spell_slots_used{};

    // Bit index = static_cast<int>(dnd::Skill) / dnd::Ability. Kept as raw
    // bitmasks here (rather than a join table) because the set of skills and
    // abilities is fixed 5e rules data, not per-character data.
    std::uint32_t skill_proficiencies = 0;
    std::uint32_t save_proficiencies = 0;
};

// The choices stored under `key` (empty if none), and whether `value` is one.
std::vector<std::string> class_option(const Character& character, const std::string& key);
bool has_class_option(const Character& character, const std::string& key, const std::string& value);
// "Metamagic=Quickened Spell|Twinned Spell;PactBoon=Pact of the Blade" and back.
std::string format_class_options(const std::map<std::string, std::vector<std::string>>& options);
std::map<std::string, std::vector<std::string>> parse_class_options(const std::string& text);

}  // namespace dnd
