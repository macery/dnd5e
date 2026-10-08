// Monster AI (docs/monster-ai.md): how each kind of mind picks its targets.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/encounter.hpp"
#include "dnd/repository.hpp"
#include "dnd/tactics.hpp"

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

    // A level-1 fighter with 12 max HP, at `hp`.
    dnd::Character fighter(int id, const std::string& name, int hp = 12) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = name;
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == "Fighter") c.class_id = entry.id;
        }
        c.strength = 16;
        c.dexterity = 12;
        c.constitution = 14;
        c.max_hp = 12;
        c.current_hp = hp;
        return c;
    }

    // The mind of a combatant made from `name`'s stat block.
    dnd::Mind mind_of(const std::string& name) {
        ScriptedRandomSource rolls({10});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, repo.get_classes(), repo.get_races());
        encounter.add_monsters(monster(name), 1);
        return dnd::creature_mind(encounter.combatants().front());
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

// Aria at full HP and Bram at 3 of 12 face one `monster`, which acts first.
// Returns the log of its turn.
std::vector<std::string> first_turn(Fixture& f, const std::string& monster, int target_roll) {
    // Initiative: Aria 1, Bram 1, the monster 20; then its target roll; then a d20 of 1 (a miss).
    ScriptedRandomSource rolls({1, 1, 20, target_roll, 1});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());
    encounter.add_character(f.fighter(1, "Aria"), {}, {});
    encounter.add_character(f.fighter(2, "Bram", 3), {}, {});
    encounter.add_monsters(f.monster(monster), 1);
    encounter.roll_initiative();
    return logs(encounter.take_events());
}

}  // namespace

TEST_CASE("a creature's mind comes from its type and Intelligence", "[monster-ai]") {
    Fixture f;
    REQUIRE(f.mind_of("Wolf") == dnd::Mind::Simple);         // a beast
    REQUIRE(f.mind_of("Flesh Golem") == dnd::Mind::Simple);  // a construct, INT 6
    REQUIRE(f.mind_of("Zombie") == dnd::Mind::Simple);       // INT 3
    REQUIRE(f.mind_of("Ogre") == dnd::Mind::Simple);         // INT 5
    REQUIRE(f.mind_of("Goblin") == dnd::Mind::Cunning);      // INT 10
    REQUIRE(f.mind_of("Young White Dragon") == dnd::Mind::Cunning);  // INT 6
    REQUIRE(f.mind_of("Bandit Captain") == dnd::Mind::Tactical);     // INT 14
    REQUIRE(f.mind_of("Mage") == dnd::Mind::Tactical);               // INT 17
}

TEST_CASE("target weights: the wounded, then casters and concentration", "[monster-ai]") {
    dnd::Combatant fresh;
    fresh.is_character = true;
    fresh.class_name = "Fighter";
    fresh.max_hp = 20;
    fresh.current_hp = 20;
    REQUIRE(dnd::target_weight(dnd::Mind::Simple, fresh) == 1);
    REQUIRE(dnd::target_weight(dnd::Mind::Cunning, fresh) == 2);
    REQUIRE(dnd::target_weight(dnd::Mind::Tactical, fresh) == 2);

    dnd::Combatant bloodied = fresh;
    bloodied.current_hp = 10;
    REQUIRE(dnd::target_weight(dnd::Mind::Cunning, bloodied) == 5);
    dnd::Combatant nearly_down = fresh;
    nearly_down.current_hp = 5;
    REQUIRE(dnd::target_weight(dnd::Mind::Cunning, nearly_down) == 7);
    REQUIRE(dnd::target_weight(dnd::Mind::Simple, nearly_down) == 1);

    dnd::Combatant cleric = fresh;
    cleric.class_name = "Cleric";
    cleric.concentration = "Bless";
    REQUIRE(dnd::is_spellcaster(cleric));
    REQUIRE_FALSE(dnd::is_spellcaster(fresh));
    REQUIRE(dnd::target_weight(dnd::Mind::Cunning, cleric) == 2);   // a cunning mind doesn't care
    REQUIRE(dnd::target_weight(dnd::Mind::Tactical, cleric) == 8);  // 2 + 4 concentrating + 2 caster
}

TEST_CASE("a cunning goblin favors the wounded; a wolf doesn't care", "[monster-ai]") {
    Fixture f;
    // The goblin's weights: Aria 2, Bram 7 (3 of 12 HP), so it rolls a d9: 1-2 Aria, 3-9 Bram.
    REQUIRE(contains(first_turn(f, "Goblin", 2), "Goblin attacks Aria"));
    REQUIRE(contains(first_turn(f, "Goblin", 3), "Goblin attacks Bram"));
    REQUIRE(contains(first_turn(f, "Goblin", 9), "Goblin attacks Bram"));
    // A wolf weighs them the same: a d2, as before.
    REQUIRE(contains(first_turn(f, "Wolf", 1), "Wolf attacks Aria"));
    REQUIRE(contains(first_turn(f, "Wolf", 2), "Wolf attacks Bram"));
}

TEST_CASE("the status tooltip says how a creature picks its targets", "[monster-ai]") {
    Fixture f;
    ScriptedRandomSource rolls({10});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());
    encounter.add_monsters(f.monster("Mage"), 1);
    encounter.add_monsters(f.monster("Wolf"), 1);
    auto mage = encounter.status_tooltip(0);
    auto wolf = encounter.status_tooltip(1);
    REQUIRE(mage.back() == "Fights tactically: goes after the wounded, spellcasters, and anyone concentrating.");
    REQUIRE(wolf.back() == "Fights by instinct: any target will do.");
}
