#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "dnd/character_creation.hpp"
#include "dnd/equipment.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"

namespace {

using dnd::Ability;
using dnd::Skill;

class SequenceRandomSource : public dnd::IRandomSource {
public:
    explicit SequenceRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int /*sides*/) override { return values_[index_++ % values_.size()]; }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

bool has(const std::vector<Skill>& skills, Skill skill) {
    return std::find(skills.begin(), skills.end(), skill) != skills.end();
}

// A valid Human Fighter (Soldier) with the standard array.
dnd::CreationChoices fighter_choices() {
    dnd::CreationChoices choices;
    choices.name = "Aria";
    choices.class_name = "Fighter";
    choices.race_name = "Human";
    choices.background_name = "Soldier";  // Athletics, Intimidation
    choices.method = dnd::AbilityScoreMethod::StandardArray;
    choices.base_scores = {15, 13, 14, 8, 12, 10};  // STR DEX CON INT WIS CHA
    choices.chosen_skills = {Skill::Perception, Skill::Survival};
    choices.fighting_style = "Defense";
    return choices;
}

}  // namespace

TEST_CASE("class, race, and background data cover the seeded tables", "[creation]") {
    dnd::Repository repo(":memory:");

    for (const auto& entry : repo.get_classes()) {
        INFO(entry.name);
        const dnd::ClassInfo* info = dnd::find_class_info(entry.name);
        REQUIRE(info != nullptr);
        REQUIRE(info->skill_choice_count <= static_cast<int>(info->skill_options.size()));
        REQUIRE(info->saving_throws[0] != info->saving_throws[1]);

        // The suggested weapon is a seeded weapon item, and the kit's main (first) weapon.
        auto items = repo.get_items();
        bool weapon_seeded = std::any_of(items.begin(), items.end(), [&](const dnd::Item& item) {
            return item.name == info->suggested_weapon && item.weapon.has_value();
        });
        REQUIRE(weapon_seeded);
        const dnd::ClassEquipment* kit = dnd::class_equipment(info->name);
        REQUIRE(kit != nullptr);
        std::string first_weapon;
        for (const auto& starting : kit->starting_kit) {
            bool is_weapon = std::any_of(items.begin(), items.end(), [&](const dnd::Item& item) {
                return item.name == starting.item_name && item.weapon.has_value();
            });
            if (is_weapon) {
                first_weapon = starting.item_name;
                break;
            }
        }
        REQUIRE(first_weapon == info->suggested_weapon);
    }
    for (const auto& entry : repo.get_races()) {
        INFO(entry.name);
        REQUIRE(dnd::find_race_info(entry.name) != nullptr);
    }
    REQUIRE(dnd::class_infos().size() == repo.get_classes().size());
    REQUIRE(dnd::race_infos().size() == repo.get_races().size());
    REQUIRE(dnd::find_background_info("Sage") != nullptr);
    REQUIRE(dnd::find_class_info("Artificer") == nullptr);
    auto classes = repo.get_classes();
    REQUIRE(dnd::find_class_info_by_id(classes, classes.front().id)->name == classes.front().name);
    REQUIRE(dnd::find_class_info_by_id(classes, 9999) == nullptr);

    // progression now reads hit dice from the same class data.
    REQUIRE(dnd::hit_die_for_class("Barbarian") == 12);
    REQUIRE(dnd::hit_die_for_class("Wizard") == 6);

    auto items = repo.get_items();
    for (const auto& starting : dnd::explorers_pack()) {
        INFO(starting.item_name);
        REQUIRE(std::any_of(items.begin(), items.end(),
                            [&](const dnd::Item& item) { return item.name == starting.item_name; }));
    }
}

TEST_CASE("roll_ability_score is 4d6 drop the lowest", "[creation]") {
    SequenceRandomSource sequence({1, 6, 3, 5});
    dnd::Dice dice(sequence);
    REQUIRE(dnd::roll_ability_score(dice) == 14);  // 6 + 5 + 3

    SequenceRandomSource all_sixes({6});
    dnd::Dice sixes(all_sixes);
    auto scores = dnd::roll_ability_scores(sixes);
    REQUIRE(std::all_of(scores.begin(), scores.end(), [](int s) { return s == 18; }));
}

TEST_CASE("point buy costs match the PHB table", "[creation]") {
    REQUIRE(dnd::point_buy_cost(8) == 0);
    REQUIRE(dnd::point_buy_cost(13) == 5);
    REQUIRE(dnd::point_buy_cost(14) == 7);
    REQUIRE(dnd::point_buy_cost(15) == 9);
    REQUIRE_THROWS_AS(dnd::point_buy_cost(7), std::invalid_argument);
    REQUIRE_THROWS_AS(dnd::point_buy_cost(16), std::invalid_argument);
    REQUIRE(dnd::point_buy_total({15, 15, 15, 8, 8, 8}) == 27);
}

TEST_CASE("validate_ability_scores per method", "[creation]") {
    auto choices = fighter_choices();
    REQUIRE_FALSE(dnd::validate_ability_scores(choices));

    SECTION("standard array values must each be used once") {
        choices.base_scores = {15, 15, 14, 8, 12, 10};
        REQUIRE(dnd::validate_ability_scores(choices));
    }

    SECTION("rolled scores must match the rolled pool") {
        choices.method = dnd::AbilityScoreMethod::Rolled;
        choices.score_pool = {17, 9, 12, 12, 6, 14};
        choices.base_scores = {17, 14, 12, 12, 9, 6};
        REQUIRE_FALSE(dnd::validate_ability_scores(choices));
        choices.base_scores = {17, 14, 12, 12, 9, 9};
        REQUIRE(dnd::validate_ability_scores(choices));
    }

    SECTION("point buy must stay within 8-15 and 27 points") {
        choices.method = dnd::AbilityScoreMethod::PointBuy;
        choices.base_scores = {15, 15, 15, 8, 8, 8};
        REQUIRE_FALSE(dnd::validate_ability_scores(choices));
        choices.base_scores = {15, 15, 15, 9, 8, 8};  // 28 points
        REQUIRE(dnd::validate_ability_scores(choices));
        choices.base_scores = {16, 8, 8, 8, 8, 8};
        REQUIRE(dnd::validate_ability_scores(choices));
    }
}

TEST_CASE("Half-Elf flexible bonuses", "[creation]") {
    auto choices = fighter_choices();
    choices.race_name = "Half-Elf";

    choices.flexible_bonus_abilities = {Ability::Strength};
    REQUIRE(dnd::validate_flexible_bonuses(choices));  // needs two
    choices.flexible_bonus_abilities = {Ability::Strength, Ability::Strength};
    REQUIRE(dnd::validate_flexible_bonuses(choices));  // must differ
    choices.flexible_bonus_abilities = {Ability::Strength, Ability::Charisma};
    REQUIRE(dnd::validate_flexible_bonuses(choices));  // CHA already +2
    choices.flexible_bonus_abilities = {Ability::Strength, Ability::Constitution};
    REQUIRE_FALSE(dnd::validate_flexible_bonuses(choices));

    auto scores = dnd::final_ability_scores(choices);
    REQUIRE(scores[0] == 16);  // STR 15 + 1
    REQUIRE(scores[2] == 15);  // CON 14 + 1
    REQUIRE(scores[5] == 12);  // CHA 10 + 2

    choices.race_name = "Human";
    REQUIRE(dnd::validate_flexible_bonuses(choices));  // Human gets none
}

TEST_CASE("skill_plan combines class, race, and background", "[creation]") {
    const auto& rogue = *dnd::find_class_info("Rogue");
    const auto& elf = *dnd::find_race_info("Elf");  // Perception

    SECTION("background skills are granted and removed from the class list") {
        auto plan = dnd::skill_plan(rogue, elf, *dnd::find_background_info("Criminal"));
        REQUIRE(has(plan.granted, Skill::Perception));
        REQUIRE(has(plan.granted, Skill::Stealth));
        REQUIRE(has(plan.granted, Skill::Deception));
        REQUIRE_FALSE(has(plan.class_options, Skill::Stealth));
        REQUIRE_FALSE(has(plan.class_options, Skill::Perception));
        REQUIRE(plan.class_choice_count == 4);
        REQUIRE(plan.free_choice_count == 0);
    }

    SECTION("a background skill the race already gives becomes a free choice") {
        auto plan = dnd::skill_plan(rogue, elf, *dnd::find_background_info("Sailor"));  // Athletics, Perception
        REQUIRE(plan.free_choice_count == 1);
    }

    SECTION("Half-Elf gets two free choices") {
        auto plan = dnd::skill_plan(rogue, *dnd::find_race_info("Half-Elf"), *dnd::find_background_info("Sage"));
        REQUIRE(plan.free_choice_count == 2);
    }
}

TEST_CASE("validate_skills", "[creation]") {
    auto choices = fighter_choices();
    REQUIRE_FALSE(dnd::validate_skills(choices));

    choices.chosen_skills = {Skill::Perception};
    REQUIRE(dnd::validate_skills(choices));  // too few

    choices.chosen_skills = {Skill::Perception, Skill::Athletics};
    REQUIRE(dnd::validate_skills(choices));  // Athletics is from Soldier

    choices.chosen_skills = {Skill::Perception, Skill::Arcana};
    REQUIRE(dnd::validate_skills(choices));  // Arcana isn't a Fighter skill

    choices.chosen_skills = {Skill::Perception, Skill::Perception};
    REQUIRE(dnd::validate_skills(choices));

    SECTION("free choices may come from outside the class list") {
        choices.race_name = "Half-Elf";
        choices.chosen_skills = {Skill::Perception, Skill::Survival, Skill::Arcana, Skill::Stealth};
        REQUIRE_FALSE(dnd::validate_skills(choices));
        choices.chosen_skills = {Skill::Perception, Skill::Arcana, Skill::Stealth, Skill::Deception};
        REQUIRE(dnd::validate_skills(choices));  // only one from the Fighter list
    }
}

TEST_CASE("build_character assembles a full level-1 character", "[creation]") {
    auto choices = fighter_choices();
    choices.class_id = 5;
    choices.race_id = 8;
    choices.alignment_id = 2;
    choices.player_id = 3;

    dnd::Character character = dnd::build_character(choices);
    REQUIRE(character.character_name == "Aria");
    REQUIRE(character.class_id == 5);
    REQUIRE(character.race == 8);
    REQUIRE(character.alignment == 2);
    REQUIRE(character.player_id == 3);
    REQUIRE(character.background == "Soldier");
    REQUIRE(character.level == 1);

    // Standard array + Human +1 to everything.
    REQUIRE(character.strength == 16);
    REQUIRE(character.dexterity == 14);
    REQUIRE(character.constitution == 15);
    REQUIRE(character.intelligence == 9);
    REQUIRE(character.wisdom == 13);
    REQUIRE(character.charisma == 11);

    REQUIRE(dnd::has_save_proficiency(character, Ability::Strength));
    REQUIRE(dnd::has_save_proficiency(character, Ability::Constitution));
    REQUIRE_FALSE(dnd::has_save_proficiency(character, Ability::Dexterity));

    for (Skill skill : {Skill::Athletics, Skill::Intimidation, Skill::Perception, Skill::Survival}) {
        REQUIRE(dnd::has_skill_proficiency(character, skill));
    }
    REQUIRE_FALSE(dnd::has_skill_proficiency(character, Skill::Stealth));

    REQUIRE(character.max_hp == 12);  // d10 + CON +2
    REQUIRE(character.current_hp == 12);
    REQUIRE(character.fighting_style == "Defense");

    choices.name = "   ";
    REQUIRE_THROWS_AS(dnd::build_character(choices), std::invalid_argument);
}

TEST_CASE("Fighters choose a Fighting Style; other classes don't", "[creation][class_features]") {
    auto choices = fighter_choices();
    choices.fighting_style = "";
    REQUIRE(dnd::validate_fighting_style(choices) == "Choose a Fighting Style.");
    REQUIRE(dnd::validate_choices(choices) == "Choose a Fighting Style.");
    choices.fighting_style = "Juggling";
    REQUIRE(dnd::validate_fighting_style(choices).has_value());
    choices.fighting_style = "Archery";
    REQUIRE_FALSE(dnd::validate_fighting_style(choices).has_value());

    choices.class_name = "Barbarian";
    choices.chosen_skills = {Skill::Perception, Skill::Survival};
    REQUIRE_FALSE(dnd::validate_fighting_style(choices).has_value());
    REQUIRE(dnd::build_character(choices).fighting_style.empty());  // ignored for other classes
}

TEST_CASE("a built character round-trips through the repository", "[creation][repository]") {
    dnd::Repository repo(":memory:");
    auto choices = fighter_choices();
    choices.player_id = repo.add_player("Aria's player");
    choices.class_id = 1;
    choices.race_id = 1;
    choices.alignment_id = 1;

    int id = repo.add_character(dnd::build_character(choices));
    auto loaded = repo.get_characters();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].character_id == id);
    REQUIRE(loaded[0].background == "Soldier");
    REQUIRE(loaded[0].max_hp == 12);
}
