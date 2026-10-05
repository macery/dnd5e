#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/checks.hpp"
#include "dnd/encounter.hpp"
#include "dnd/repository.hpp"
#include "dnd/scenes.hpp"

namespace {

class ScriptedRandomSource : public dnd::IRandomSource {
public:
    explicit ScriptedRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int sides) override {
        int value = index_ < values_.size() ? values_[index_++] : values_.back();
        return std::min(value, sides);
    }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

struct Party {
    dnd::Repository repo{":memory:"};
    int player = repo.add_player("Adam");

    int add(const std::string& name, const std::string& class_name, std::uint32_t skills = 0) {
        dnd::Character c;
        c.character_name = name;
        c.player_id = player;
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == class_name) c.class_id = entry.id;
        }
        c.race = repo.get_races().front().id;
        c.alignment = repo.get_alignments().front().id;
        c.level = 3;
        c.strength = 16;   // +3
        c.dexterity = 14;  // +2
        c.wisdom = 12;     // +1
        c.max_hp = 30;
        c.current_hp = 30;
        c.skill_proficiencies = skills;
        return repo.add_character(c);
    }

    dnd::Character get(int id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == id) return c;
        }
        return {};
    }
};

std::uint32_t skill(dnd::Skill s) {
    return 1u << static_cast<int>(s);
}

}  // namespace

TEST_CASE("ability checks", "[scenes][checks]") {
    Party p;
    int aria = p.add("Aria", "Fighter", skill(dnd::Skill::Athletics));
    auto rollers = dnd::party_rollers(p.repo, {aria});
    REQUIRE(rollers.size() == 1);
    REQUIRE(dnd::check_bonus(rollers[0], "Athletics") == 3 + 2);  // STR + proficiency
    REQUIRE(dnd::check_bonus(rollers[0], "Stealth") == 2);         // DEX, not proficient
    REQUIRE(dnd::check_bonus(rollers[0], "Strength") == 3);
    REQUIRE(dnd::passive_score(rollers[0], "Perception") == 11);

    ScriptedRandomSource rolls({10, 4});
    dnd::Dice dice(rolls);
    auto plain = dnd::ability_check(dice, rollers[0], "Athletics", 15);
    REQUIRE(plain.total == 15);
    REQUIRE(plain.success);
    REQUIRE(plain.text() == "Aria: Athletics d20(10) +5 = 15 vs DC 15 -> success");

    SECTION("Help gives advantage; exhaustion disadvantage") {
        ScriptedRandomSource two({4, 17});
        dnd::Dice d(two);
        auto helped = dnd::ability_check(d, rollers[0], "Athletics", 15, true);
        REQUIRE(helped.d20.result == 17);
        REQUIRE(helped.text().find("with advantage (Help)") != std::string::npos);
        rollers[0].character.exhaustion = 1;
        REQUIRE(dnd::check_mode(rollers[0], "Athletics", false, false).mode == dnd::RollMode::Disadvantage);
        REQUIRE(dnd::check_mode(rollers[0], "Athletics", true, false).mode == dnd::RollMode::Normal);  // they cancel
    }
    SECTION("group checks pass when half succeed") {
        int bram = p.add("Bram", "Fighter");
        auto both = dnd::party_rollers(p.repo, {aria, bram});
        ScriptedRandomSource group({15, 2});
        dnd::Dice d(group);
        auto result = dnd::group_check(d, both, "Stealth", 14);
        REQUIRE(result.rolls.size() == 2);
        REQUIRE(result.success);  // 17 and 4: one of two
    }
}

TEST_CASE("scene outcomes change the party and the run", "[scenes]") {
    Party p;
    int aria = p.add("Aria", "Fighter");
    int bram = p.add("Bram", "Rogue");
    dnd::AdventureRun run;
    run.party = {aria, bram};

    dnd::SceneOutcome outcome;
    outcome.text = "The bridge gives way under Aria.";
    outcome.damage_dice = 2;
    outcome.damage_sides = 6;
    outcome.inspiration = true;
    outcome.loot = {{"Potion of Healing", 1}};
    outcome.coins_cp = 1000;
    outcome.unlock = "knows-the-password";
    outcome.surprise = dnd::Surprise::PartySurprises;
    outcome.fewer_monsters = 1;
    ScriptedRandomSource rolls({4, 3});
    dnd::Dice dice(rolls);
    auto lines = dnd::apply_outcome(p.repo, run, outcome, aria, dice);
    REQUIRE(lines.front() == "The bridge gives way under Aria.");
    REQUIRE(p.get(aria).current_hp == 23);
    REQUIRE(p.get(bram).current_hp == 30);
    REQUIRE(p.get(aria).inspiration);
    REQUIRE(p.get(aria).coins[3] == 5);
    REQUIRE(p.get(bram).coins[3] == 5);
    REQUIRE(std::find(run.flags.begin(), run.flags.end(), "knows-the-password") != run.flags.end());

    SECTION("the next fight is shaped, and the changes are used up") {
        dnd::AdventureScene fight;
        fight.monsters = {{"Goblin", 3}, {"Wolf", 1}};
        bool skip = false;
        auto surprise = dnd::shape_next_fight(run, fight, skip);
        REQUIRE_FALSE(skip);
        REQUIRE(surprise == dnd::Surprise::PartySurprises);
        REQUIRE(fight.monsters.size() == 1);  // the wolf is gone
        REQUIRE(run.surprise == dnd::Surprise::None);
        REQUIRE(run.fewer_monsters == 0);
    }
    SECTION("options can need a flag") {
        dnd::AdventureScene vault;
        vault.kind = dnd::SceneKind::Choice;
        dnd::SceneOption open;
        open.label = "Say the password";
        open.requires_flag = "knows-the-password";
        dnd::SceneOption force;
        force.label = "Force the door";
        vault.options = {open, force};
        REQUIRE(dnd::available_options(vault, run).size() == 2);
        dnd::AdventureRun fresh;
        REQUIRE(dnd::available_options(vault, fresh).size() == 1);
    }
    SECTION("the run remembers it between sessions") {
        run.title = "Test";
        run.run_id = p.repo.add_adventure_run(run);
        auto saved = p.repo.get_adventure_runs().front();
        REQUIRE(saved.flags == run.flags);
        REQUIRE(saved.surprise == dnd::Surprise::PartySurprises);
        REQUIRE(saved.fewer_monsters == 1);
    }
}

TEST_CASE("traps: spotted by passive Perception, or sprung", "[scenes]") {
    Party p;
    int aria = p.add("Aria", "Fighter");
    int nim = p.add("Nim", "Rogue", skill(dnd::Skill::Perception));
    auto party = dnd::party_rollers(p.repo, {aria, nim});
    auto best = dnd::best_passive(party, "Perception");
    REQUIRE(best.name == "Nim");
    REQUIRE(best.score == 13);  // 10 + WIS 1 + proficiency 2

    dnd::SceneTrap blade;
    blade.save_dc = 13;
    blade.damage_dice = 2;
    blade.damage_sides = 10;
    ScriptedRandomSource rolls({6, 6, 3});  // 12 damage; Aria's DEX save d20 3 + 2 fails
    dnd::Dice dice(rolls);
    auto lines = dnd::spring_trap(p.repo, blade, {aria, nim}, aria, dice);
    REQUIRE(lines.size() == 1);
    REQUIRE(p.get(aria).current_hp == 18);
    REQUIRE(p.get(nim).current_hp == 30);
}

TEST_CASE("surprise: the surprised lose their first turn", "[scenes][encounter]") {
    Party p;
    int aria = p.add("Aria", "Fighter");
    dnd::Monster goblin;
    for (const auto& m : p.repo.get_monsters()) {
        if (m.name == "Goblin") goblin = m;
    }
    SECTION("the party surprises the goblin") {
        ScriptedRandomSource rolls({1, 20});  // the goblin would go first
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, p.repo.get_classes(), p.repo.get_races());
        encounter.set_surprise(dnd::Surprise::PartySurprises);
        encounter.add_character(p.get(aria), {}, {});
        encounter.add_monsters(goblin, 1);
        encounter.roll_initiative();
        std::string text;
        for (const auto& e : encounter.take_events()) text += e.text + "\n";
        REQUIRE(text.find("Goblin is surprised and loses this turn.") != std::string::npos);
        REQUIRE(encounter.current_index() == 0);  // straight to Aria
        REQUIRE(encounter.combatants()[0].current_hp == 30);
    }
    SECTION("inspiration gives advantage on an attack, once") {
        auto inspired = p.get(aria);
        inspired.inspiration = true;
        ScriptedRandomSource rolls({20, 1, 3, 15, 5});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, p.repo.get_classes(), p.repo.get_races());
        encounter.add_character(inspired, {}, {});
        encounter.add_monsters(goblin, 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.has_inspiration());
        const auto& me = encounter.combatants()[0];
        dnd::AttackOptions options;
        options.inspired = true;
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus, options);
        std::string text;
        for (const auto& e : encounter.take_events()) text += e.text + "\n";
        REQUIRE(text.find("with advantage (inspiration)") != std::string::npos);
        REQUIRE_FALSE(encounter.character(aria)->inspiration);
    }
}
