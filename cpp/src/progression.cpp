#include "dnd/progression.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/lasting_effects.hpp"
#include "dnd/rules.hpp"

namespace dnd {

namespace {

// Fractional CRs first, then whole CRs 1-30 (index = CR).
constexpr std::array<std::pair<double, int>, 4> kFractionalCrXp = {{
    {0.0, 10}, {0.125, 25}, {0.25, 50}, {0.5, 100},
}};

constexpr std::array<int, 31> kWholeCrXp = {
    0,  // CR 0 is handled by kFractionalCrXp
    200, 450, 700, 1100, 1800, 2300, 2900, 3900, 5000, 5900,
    7200, 8400, 10000, 11500, 13000, 15000, 18000, 20000, 22000, 25000,
    33000, 41000, 50000, 62000, 75000, 90000, 105000, 120000, 135000, 155000,
};

constexpr std::array<int, kMaxLevel> kLevelThresholds = {
    0,     300,   900,   2700,  6500,   14000,  23000,  34000,  48000,  64000,
    85000, 100000, 120000, 140000, 165000, 195000, 225000, 265000, 305000, 355000,
};


}  // namespace

int xp_for_challenge_rating(double challenge_rating) {
    for (const auto& [cr, xp] : kFractionalCrXp) {
        if (challenge_rating == cr) return xp;
    }
    double whole = std::floor(challenge_rating);
    if (whole == challenge_rating && whole >= 1 && whole < static_cast<double>(kWholeCrXp.size())) {
        return kWholeCrXp[static_cast<size_t>(whole)];
    }
    throw std::invalid_argument("challenge rating not on the 5e XP table");
}

int xp_threshold_for_level(int level) {
    if (level < 1 || level > kMaxLevel) throw std::invalid_argument("level must be in [1, 20]");
    return kLevelThresholds[static_cast<size_t>(level - 1)];
}

int level_for_xp(int xp) {
    int level = 1;
    while (level < kMaxLevel && xp >= xp_threshold_for_level(level + 1)) ++level;
    return level;
}

int hit_die_for_class(const std::string& class_name) {
    if (const ClassInfo* info = find_class_info(class_name)) return info->hit_die;
    throw std::invalid_argument("unknown class: " + class_name);
}

int hit_die_for_class_id(const std::vector<LookupEntry>& classes, int class_id) {
    for (const auto& entry : classes) {
        if (entry.id != class_id) continue;
        if (const ClassInfo* info = find_class_info(entry.name)) return info->hit_die;
    }
    return 8;
}

int level_up_hp_gain(int hit_die, int constitution) {
    return std::max(1, hit_die / 2 + 1 + ability_modifier(constitution));
}

int award_experience(Character& character, int xp) {
    int before = std::max(character.level, level_for_xp(character.experience));
    character.experience += xp;
    return std::max(0, level_for_xp(character.experience) - before);
}

int hit_dice_remaining(const Character& character) {
    return std::max(0, character.level - character.hit_dice_used);
}

int spend_hit_die(Character& character, const Dice& dice, int hit_die) {
    if (character.dead || hit_dice_remaining(character) == 0) return -1;

    ++character.hit_dice_used;
    int roll = dice.roll(hit_die, 1).front();
    int healing = std::max(0, roll + ability_modifier(character.constitution));
    int before = character.current_hp;
    character.current_hp = std::min(character.max_hp, character.current_hp + healing);
    return character.current_hp - before;
}

void take_long_rest(Character& character) {
    if (character.dead) return;  // only revival magic brings the dead back
    end_lasting_effects(character);  // Mage Armor and Aid last 8 hours
    character.max_hp += character.max_hp_reduction;  // Life Drain wears off
    character.max_hp_reduction = 0;
    character.current_hp = character.max_hp;
    int recovered = std::max(1, character.level / 2);
    character.hit_dice_used = std::max(0, character.hit_dice_used - recovered);
    character.spell_slots_used.fill(0);
    restore_all_features(character);
    character.revival_penalty = std::max(0, character.revival_penalty - 1);
    character.may_prepare = true;  // prepared spells can change now (docs/preparing-spells.md)
    character.exhaustion = std::max(0, character.exhaustion - 1);  // a long rest removes one level
    character.temp_hp = 0;
    character.haggle = 0;  // the merchant can be asked again
}

void take_short_rest(Character& character, const ClassInfo& class_info) {
    if (character.dead) return;
    if (class_info.spellcasting.caster_type == CasterType::Pact) character.spell_slots_used.fill(0);
    restore_short_rest_features(character, class_info.name);
}

ShortRestResult short_rest(Character& character, const ClassInfo& class_info, const Dice& dice, int hit_dice,
                           int song_of_rest_die) {
    ShortRestResult result;
    if (character.dead) return result;
    auto slots_before = character.spell_slots_used;
    auto features_before = character.feature_uses_spent;
    take_short_rest(character, class_info);
    result.slots_restored = character.spell_slots_used != slots_before;
    result.features_restored = character.feature_uses_spent != features_before;
    result.arcane_recovery = use_arcane_recovery(character, class_info.name);

    for (int i = 0; i < hit_dice && character.current_hp < character.max_hp; ++i) {
        int regained = spend_hit_die(character, dice, class_info.hit_die);
        if (regained < 0) break;  // none left
        ++result.hit_dice_spent;
        result.hp_regained += regained;
    }
    if (song_of_rest_die > 0 && result.hit_dice_spent > 0) {
        int before = character.current_hp;
        character.current_hp = std::min(character.max_hp, character.current_hp + dice.roll(song_of_rest_die, 1).front());
        result.song_of_rest = character.current_hp - before;
        result.hp_regained += result.song_of_rest;
    }
    return result;
}

}  // namespace dnd
