#include "dnd/progression.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

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

struct ClassHitDie {
    const char* class_name;
    int hit_die;
};

constexpr std::array<ClassHitDie, 12> kClassHitDice = {{
    {"Barbarian", 12}, {"Bard", 8},    {"Cleric", 8},   {"Druid", 8},
    {"Fighter", 10},   {"Monk", 8},    {"Paladin", 10}, {"Ranger", 10},
    {"Rogue", 8},      {"Sorcerer", 6}, {"Warlock", 8}, {"Wizard", 6},
}};

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
    for (const auto& entry : kClassHitDice) {
        if (class_name == entry.class_name) return entry.hit_die;
    }
    throw std::invalid_argument("unknown class: " + class_name);
}

int hit_die_for_class_id(const std::vector<LookupEntry>& classes, int class_id) {
    for (const auto& entry : classes) {
        if (entry.id != class_id) continue;
        for (const auto& known : kClassHitDice) {
            if (entry.name == known.class_name) return known.hit_die;
        }
    }
    return 8;
}

int level_up_hp_gain(int hit_die, int constitution) {
    return std::max(1, hit_die / 2 + 1 + ability_modifier(constitution));
}

int award_experience(Character& character, int xp, int hit_die) {
    character.experience += xp;

    int levels_gained = 0;
    int per_level = level_up_hp_gain(hit_die, character.constitution);
    while (character.level < level_for_xp(character.experience)) {
        ++character.level;
        character.max_hp += per_level;
        character.current_hp += per_level;
        ++levels_gained;
    }
    return levels_gained;
}

int hit_dice_remaining(const Character& character) {
    return std::max(0, character.level - character.hit_dice_used);
}

int spend_hit_die(Character& character, const Dice& dice, int hit_die) {
    if (hit_dice_remaining(character) == 0) return -1;

    ++character.hit_dice_used;
    int roll = dice.roll(hit_die, 1).front();
    int healing = std::max(0, roll + ability_modifier(character.constitution));
    int before = character.current_hp;
    character.current_hp = std::min(character.max_hp, character.current_hp + healing);
    return character.current_hp - before;
}

void take_long_rest(Character& character) {
    character.current_hp = character.max_hp;
    int recovered = std::max(1, character.level / 2);
    character.hit_dice_used = std::max(0, character.hit_dice_used - recovered);
}

}  // namespace dnd
