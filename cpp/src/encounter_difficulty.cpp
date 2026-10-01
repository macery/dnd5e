#include "dnd/encounter_difficulty.hpp"

#include <algorithm>
#include <cmath>

#include "dnd/progression.hpp"

namespace dnd {

namespace {

// DMG ch. 3, "XP Thresholds by Character Level".
constexpr std::array<std::array<int, 4>, 20> kThresholds = {{
    {25, 50, 75, 100},         {50, 100, 150, 200},       {75, 150, 225, 400},       {125, 250, 375, 500},
    {250, 500, 750, 1100},     {300, 600, 900, 1400},     {350, 750, 1100, 1700},    {450, 900, 1400, 2100},
    {550, 1100, 1600, 2400},   {600, 1200, 1900, 2800},   {800, 1600, 2400, 3600},   {1000, 2000, 3000, 4500},
    {1100, 2200, 3400, 5100},  {1250, 2500, 3800, 5700},  {1400, 2800, 4300, 6400},  {1600, 3200, 4800, 7200},
    {2000, 3900, 5900, 8800},  {2100, 4200, 6300, 9500},  {2400, 4900, 7300, 10900}, {2800, 5700, 8500, 12700},
}};

constexpr std::array<double, 8> kMultipliers = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0};

std::string with_commas(int value) {
    std::string digits = std::to_string(value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) digits.insert(static_cast<size_t>(i), ",");
    return digits;
}

int monster_xp(const std::vector<Monster>& monsters, const std::string& name) {
    for (const auto& monster : monsters) {
        if (monster.name == name) return xp_for_challenge_rating(monster.challenge_rating);
    }
    return 0;
}

EncounterRating rate_groups(const std::vector<AdventureMonsterGroup>& groups, const std::vector<int>& party_levels,
                            const std::vector<Monster>& monsters) {
    std::vector<int> xp;
    for (const auto& group : groups) {
        for (int n = 0; n < group.count; ++n) xp.push_back(monster_xp(monsters, group.monster_name));
    }
    return rate_encounter(party_levels, xp);
}

}  // namespace

const char* difficulty_name(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Trivial: return "Trivial";
        case Difficulty::Easy: return "Easy";
        case Difficulty::Medium: return "Medium";
        case Difficulty::Hard: return "Hard";
        case Difficulty::Deadly: return "Deadly";
    }
    return "Trivial";
}

std::array<int, 4> xp_thresholds(int level) {
    return kThresholds[static_cast<size_t>(std::clamp(level, 1, 20) - 1)];
}

double encounter_multiplier(int monster_count, int party_size) {
    if (monster_count <= 0) return 1.0;
    int step = monster_count == 1 ? 1 : monster_count == 2 ? 2 : monster_count <= 6 ? 3 : monster_count <= 10 ? 4
             : monster_count <= 14 ? 5 : 6;
    if (party_size > 0 && party_size < 3) ++step;
    if (party_size >= 6) --step;
    return kMultipliers[static_cast<size_t>(std::clamp(step, 0, 7))];
}

std::string EncounterRating::text() const {
    return std::string(difficulty_name(difficulty)) + ": " + with_commas(adjusted_xp) + " adjusted XP (Easy " +
           with_commas(party_thresholds[0]) + ", Medium " + with_commas(party_thresholds[1]) + ", Hard " +
           with_commas(party_thresholds[2]) + ", Deadly " + with_commas(party_thresholds[3]) + ")";
}

EncounterRating rate_encounter(const std::vector<int>& party_levels, const std::vector<int>& monster_xp) {
    EncounterRating rating;
    for (int level : party_levels) {
        auto thresholds = xp_thresholds(level);
        for (size_t i = 0; i < 4; ++i) rating.party_thresholds[i] += thresholds[i];
    }
    for (int xp : monster_xp) rating.base_xp += xp;
    rating.multiplier = encounter_multiplier(static_cast<int>(monster_xp.size()), static_cast<int>(party_levels.size()));
    rating.adjusted_xp = static_cast<int>(std::lround(rating.base_xp * rating.multiplier));

    rating.difficulty = Difficulty::Trivial;
    if (party_levels.empty()) return rating;
    const Difficulty bands[] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Deadly};
    for (size_t i = 0; i < 4; ++i) {
        if (rating.adjusted_xp >= rating.party_thresholds[i]) rating.difficulty = bands[i];
    }
    return rating;
}

ScaledScene scale_scene(const AdventureScene& scene, const std::vector<int>& party_levels,
                        const std::vector<Monster>& monsters) {
    ScaledScene result;
    result.scene = scene;
    auto& groups = result.scene.monsters;
    auto rate = [&] { return rate_groups(groups, party_levels, monsters); };
    result.rating = rate();
    if (party_levels.empty()) return result;
    int level_sum = 0;
    for (int level : party_levels) level_sum += level;
    int average = std::max(1, static_cast<int>(std::lround(static_cast<double>(level_sum) / party_levels.size())));
    result.scene.coins_cp = scene.coins_cp * average;
    if (groups.empty()) return result;

    auto xp_of = [&](const AdventureMonsterGroup& group) { return monster_xp(monsters, group.monster_name); };
    constexpr int kMaxMonsters = 12;  // keep fights playable on one screen
    auto total = [&] {
        int count = 0;
        for (const auto& group : groups) count += group.count;
        return count;
    };
    Difficulty target = scene.boss ? Difficulty::Hard : Difficulty::Medium;

    // Too hard: remove the most expensive monster from a group with more than one.
    while (result.rating.difficulty == Difficulty::Deadly) {
        AdventureMonsterGroup* pick = nullptr;
        for (auto& group : groups) {
            if (group.count > 1 && (!pick || xp_of(group) > xp_of(*pick))) pick = &group;
        }
        if (!pick) break;
        --pick->count;
        result.rating = rate();
    }

    // Too easy: add the cheapest monster that doesn't make it Deadly.
    while (result.rating.difficulty < target && total() < kMaxMonsters) {
        AdventureMonsterGroup* pick = nullptr;
        for (auto& group : groups) {
            if (!pick || xp_of(group) < xp_of(*pick)) pick = &group;
        }
        ++pick->count;
        EncounterRating next = rate();
        if (next.difficulty == Difficulty::Deadly) {
            --pick->count;
            break;
        }
        result.rating = next;
    }

    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].count != scene.monsters[i].count) result.scaled = true;
    }
    return result;
}

ScaledAdventure scale_adventure(const AdventureTemplate& adventure, const std::vector<int>& party_levels,
                                const std::vector<Monster>& monsters) {
    ScaledAdventure result;
    result.adventure = adventure;
    for (size_t i = 0; i < adventure.scenes.size(); ++i) {
        ScaledScene scaled = scale_scene(adventure.scenes[i], party_levels, monsters);
        result.adventure.scenes[i] = scaled.scene;
        result.ratings.push_back(scaled.rating);
        result.hardest = std::max(result.hardest, scaled.rating.difficulty);
        result.scaled = result.scaled || scaled.scaled;
    }
    return result;
}

}  // namespace dnd
