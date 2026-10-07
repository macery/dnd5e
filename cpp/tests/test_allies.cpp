// NPC allies in fights (docs/scenes-and-npcs.md, phase 3): a stat block on
// the party's side, run by the monster AI.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/adventure_run.hpp"
#include "dnd/encounter.hpp"
#include "dnd/repository.hpp"
#include "dnd/scenes.hpp"

namespace {

using dnd::EncounterEvent;

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

struct Fixture {
    dnd::Repository repo{":memory:"};

    dnd::Monster monster(const std::string& name) {
        for (const auto& m : repo.get_monsters()) {
            if (m.name == name) return m;
        }
        FAIL("no monster " << name);
        return {};
    }

    dnd::Item item(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i;
        }
        FAIL("no item " << name);
        return {};
    }

    dnd::Spell spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return {};
    }

    // A level-1 fighter in chain mail with a longsword: AC 16, 12 HP.
    dnd::Character fighter() {
        dnd::Character c;
        c.character_id = 1;
        c.character_name = "Aria";
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == "Fighter") c.class_id = entry.id;
        }
        c.strength = 16;
        c.dexterity = 12;
        c.constitution = 14;
        c.max_hp = 12;
        c.current_hp = 12;
        return c;
    }

    std::vector<dnd::InventoryEntry> gear() {
        return {{item("Chain Mail"), 1, true}, {item("Longsword"), 1, true}};
    }

    dnd::Encounter encounter(const dnd::Dice& dice) {
        return dnd::Encounter(dice, repo.get_classes(), repo.get_races());
    }
};

std::vector<std::string> logs(const std::vector<EncounterEvent>& events) {
    std::vector<std::string> lines;
    for (const auto& e : events) {
        if (e.kind == EncounterEvent::Kind::Log) lines.push_back(e.text);
    }
    return lines;
}

bool contains(const std::vector<std::string>& lines, const std::string& needle) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& line) { return line.find(needle) != std::string::npos; });
}

}  // namespace

TEST_CASE("an ally fights the monsters, isn't a target for the party, and earns no XP", "[allies]") {
    Fixture f;
    // Initiative: Aria 20 (+1), Dagna 1 (+1), Goblin 2 (+2). Then every roll is a 20: the goblin
    // picks the second of its two targets (Dagna), and everyone crits.
    ScriptedRandomSource rolls({20, 1, 2, 20});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.gear());
    encounter.add_ally(f.monster("Veteran"), "Dagna Ironfoot (Veteran)", 7);
    encounter.add_monsters(f.monster("Goblin"), 1);

    const auto& dagna = encounter.combatants()[1];
    REQUIRE(dagna.ally);
    REQUIRE(dagna.npc_id == 7);
    REQUIRE(dagna.xp == 0);
    REQUIRE(dnd::party_side(dagna));
    // Only the goblin counts toward how hard the fight is.
    auto rated = encounter.difficulty();
    dnd::Encounter goblin_only = f.encounter(dice);
    goblin_only.add_character(f.fighter(), {}, f.gear());
    goblin_only.add_monsters(f.monster("Goblin"), 1);
    REQUIRE(rated.adjusted_xp == goblin_only.difficulty().adjusted_xp);

    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current_index() == 0);
    REQUIRE(encounter.attack_targets() == std::vector<int>{2});  // the goblin, never Dagna
    // Healing spells reach allies too.
    auto heal = encounter.spell_target_candidates(f.spell("Cure Wounds"));
    REQUIRE(std::find(heal.begin(), heal.end(), 1) != heal.end());

    encounter.next_turn();  // the goblin goes for Dagna; then Dagna cuts it down
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Goblin attacks Dagna Ironfoot (Veteran)"));
    REQUIRE(contains(lines, "Dagna Ironfoot (Veteran) attacks Goblin"));
    REQUIRE(encounter.status() == dnd::EncounterStatus::Victory);
    REQUIRE(encounter.character(1)->experience == 50);  // the goblin's XP, all Aria's
    REQUIRE(encounter.fallen_allies().empty());
}

TEST_CASE("a downed ally doesn't lose the fight, and dies if the party does", "[allies]") {
    Fixture f;
    // Initiative: Aria 1 (+1), Snig 1 (+2), Ogre 20 (-1). The ogre picks Snig, crits, and drops it;
    // next round it crits Aria for far more than her maximum HP.
    ScriptedRandomSource rolls({1, 1, 20, 2, 20});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.gear());
    encounter.add_ally(f.monster("Goblin"), "Snig (Goblin)", 3);
    encounter.add_monsters(f.monster("Ogre"), 1);

    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Ogre attacks Snig (Goblin)"));
    REQUIRE(encounter.combatants()[1].current_hp == 0);
    REQUIRE(encounter.status_text(1) == "Down");
    REQUIRE(encounter.status() == dnd::EncounterStatus::InProgress);  // Aria's still up
    REQUIRE(encounter.current_index() == 0);

    encounter.next_turn();  // Snig is down and skipped; the ogre flattens Aria
    lines = logs(encounter.take_events());
    REQUIRE(encounter.status() == dnd::EncounterStatus::Defeat);
    REQUIRE(contains(lines, "Snig (Goblin) is left behind, and dies."));
    REQUIRE(encounter.fallen_allies() == std::vector<int>{3});
}

TEST_CASE("the run remembers who fights beside the party next, until that fight", "[allies][adventure]") {
    dnd::AdventureRun run;
    run.allies = {"snig", "dagna"};
    run.surprise = dnd::Surprise::PartySurprises;
    std::string text = dnd::format_next_fight(run);
    REQUIRE(text == "surprise=party;allies=snig,dagna;");
    dnd::AdventureRun back;
    dnd::parse_next_fight(back, text);
    REQUIRE(back.allies == run.allies);
    REQUIRE(back.surprise == dnd::Surprise::PartySurprises);
    dnd::parse_next_fight(back, "");
    REQUIRE(back.allies.empty());

    const auto* warren = dnd::find_adventure("The Goblin Warren");
    run.title = warren->title;
    run.next_scene = 5;
    dnd::record_fight(run, *warren, true);
    REQUIRE(run.allies.empty());  // used up
}

TEST_CASE("outcomes bring NPCs into the next fight: alive, not Hostile, with a stat block", "[allies][scenes]") {
    Fixture f;
    int player = f.repo.add_player("Adam");
    dnd::Character aria = f.fighter();
    aria.player_id = player;
    aria.race = f.repo.get_races().front().id;
    aria.alignment = f.repo.get_alignments().front().id;
    int aria_id = f.repo.add_character(aria);
    ScriptedRandomSource rolls({10});
    dnd::Dice dice(rolls);

    dnd::AdventureRun run;
    run.title = "The Hill Giant's Toll";
    run.party = {aria_id};
    dnd::SceneOutcome joins;
    joins.ally_joins = "dagna";
    auto lines = dnd::apply_outcome(f.repo, run, joins, aria_id, dice);
    REQUIRE(run.allies == std::vector<std::string>{"dagna"});
    REQUIRE(contains(lines, "Dagna Ironfoot will fight beside the party"));
    dnd::apply_outcome(f.repo, run, joins, aria_id, dice);
    REQUIRE(run.allies.size() == 1);  // once

    auto allies = dnd::next_fight_allies(f.repo, run, f.repo.get_monsters());
    REQUIRE(allies.size() == 1);
    REQUIRE(allies.front().npc.name == "Dagna Ironfoot");
    REQUIRE(allies.front().stat_block.name == "Veteran");

    // Turned Hostile since: she won't come.
    auto dagna = allies.front().npc;
    dagna.attitude = dnd::Attitude::Hostile;
    f.repo.update_npc(dagna);
    REQUIRE(dnd::next_fight_allies(f.repo, run, f.repo.get_monsters()).empty());
    dnd::AdventureRun fresh = run;
    fresh.allies.clear();
    REQUIRE(dnd::apply_outcome(f.repo, fresh, joins, aria_id, dice).empty());
    REQUIRE(fresh.allies.empty());

    // Dead: gone for good.
    dagna.attitude = dnd::Attitude::Friendly;
    f.repo.update_npc(dagna);
    dnd::mark_npcs_dead(f.repo, 0, {dagna.npc_id});
    REQUIRE(f.repo.get_npcs(0).front().dead);
    REQUIRE(dnd::next_fight_allies(f.repo, run, f.repo.get_monsters()).empty());

    // An NPC with no stat block never fights.
    dnd::AdventureRun crypt;
    crypt.title = "Crypt of the Restless";
    crypt.party = {aria_id};
    dnd::SceneOutcome tobin;
    tobin.ally_joins = "tobin";
    dnd::apply_outcome(f.repo, crypt, tobin, aria_id, dice);
    REQUIRE(crypt.allies.empty());
}

TEST_CASE("every NPC an adventure brings into a fight has a seeded stat block", "[allies][adventure]") {
    dnd::Repository repo(":memory:");
    auto monsters = repo.get_monsters();
    int joining = 0;
    for (const auto& adventure : dnd::adventure_templates()) {
        auto check = [&](const dnd::SceneOutcome& outcome) {
            if (outcome.ally_joins.empty()) return;
            ++joining;
            auto npc = std::find_if(adventure.npcs.begin(), adventure.npcs.end(),
                                    [&](const dnd::Npc& n) { return n.key == outcome.ally_joins; });
            REQUIRE(npc != adventure.npcs.end());
            REQUIRE(std::any_of(monsters.begin(), monsters.end(),
                                [&](const dnd::Monster& m) { return m.name == npc->stat_block; }));
        };
        for (const auto& scene : adventure.scenes) {
            for (const auto& option : scene.options) {
                check(option.success);
                check(option.failure);
            }
        }
    }
    REQUIRE(joining >= 2);  // Snig and Dagna
}
