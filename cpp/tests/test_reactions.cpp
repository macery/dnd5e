#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/encounter.hpp"
#include "dnd/repository.hpp"

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

struct Fight {
    dnd::Repository repo{":memory:"};

    dnd::Spell spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return {};
    }

    dnd::Monster goblin() {
        for (const auto& m : repo.get_monsters()) {
            if (m.name == "Goblin") return m;
        }
        return {};
    }

    dnd::Character hero(const std::string& class_name, int level) {
        dnd::Character c;
        c.character_id = 1;
        c.character_name = "Hero";
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == class_name) c.class_id = entry.id;
        }
        c.level = level;
        c.dexterity = 14;  // AC 12 unarmored
        c.intelligence = 16;
        c.charisma = 16;
        c.max_hp = 30;
        c.current_hp = 30;
        return c;
    }
};

std::string text_of(const std::vector<dnd::EncounterEvent>& events) {
    std::string text;
    for (const auto& e : events) {
        if (e.kind == dnd::EncounterEvent::Kind::Log) text += e.text + "\n";
    }
    return text;
}

}  // namespace

TEST_CASE("Shield turns a hit into a miss", "[reactions]") {
    Fight f;
    // Initiative: the wizard 1, the goblin 20. The goblin's scimitar: d20 10 + 4 = 14 vs AC 12.
    ScriptedRandomSource rolls({1, 20, 1, 10, 4});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());

    SECTION("automatically") {
        encounter.add_character(f.hero("Wizard", 1), {f.spell("Shield")}, {});
        encounter.add_monsters(f.goblin(), 1);
        encounter.roll_initiative();
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(text.find("Hero casts Shield as a reaction (1st-level slot): AC 17") != std::string::npos);
        REQUIRE(text.find("-> miss") != std::string::npos);
        REQUIRE(encounter.combatants()[0].current_hp == 30);
        REQUIRE(encounter.character(1)->spell_slots_used[0] == 1);
    }
    SECTION("not when reactions are off") {
        encounter.set_auto_reactions(false);
        encounter.add_character(f.hero("Wizard", 1), {f.spell("Shield")}, {});
        encounter.add_monsters(f.goblin(), 1);
        encounter.roll_initiative();
        REQUIRE(encounter.combatants()[0].current_hp < 30);
        REQUIRE(encounter.character(1)->spell_slots_used[0] == 0);
    }
    SECTION("not when +5 wouldn't be enough") {
        ScriptedRandomSource high({1, 20, 1, 19, 4});  // 23 vs AC 17: still a hit
        dnd::Dice high_dice(high);
        dnd::Encounter tough(high_dice, f.repo.get_classes(), f.repo.get_races());
        tough.add_character(f.hero("Wizard", 1), {f.spell("Shield")}, {});
        tough.add_monsters(f.goblin(), 1);
        tough.roll_initiative();
        REQUIRE(tough.character(1)->spell_slots_used[0] == 0);  // slot kept
    }
}

TEST_CASE("Uncanny Dodge halves a hit at level 5", "[reactions]") {
    Fight f;
    // The goblin hits: d20 15; 1d6 = 4 + 2 = 6, halved to 3.
    ScriptedRandomSource rolls({1, 20, 1, 15, 4});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());
    encounter.add_character(f.hero("Rogue", 5), {}, {});
    encounter.add_monsters(f.goblin(), 1);
    encounter.roll_initiative();
    auto text = text_of(encounter.take_events());
    REQUIRE(text.find("(halved by Uncanny Dodge)") != std::string::npos);
    REQUIRE(encounter.combatants()[0].current_hp == 27);
}

TEST_CASE("Hellish Rebuke burns whoever hurt the warlock", "[reactions]") {
    Fight f;
    // The goblin hits: d20 15, 1d6 4 (+2 = 6). Rebuke: 2d10 = 5 + 5; the goblin's DEX save d20 2 + 2 vs DC 13.
    ScriptedRandomSource rolls({1, 20, 1, 15, 4, 5, 5, 2});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());
    encounter.add_character(f.hero("Warlock", 1), {f.spell("Hellish Rebuke")}, {});
    encounter.add_monsters(f.goblin(), 1);
    encounter.roll_initiative();
    auto text = text_of(encounter.take_events());
    INFO(text);
    REQUIRE(text.find("Hero casts Hellish Rebuke as a reaction (1st-level slot) at Goblin") != std::string::npos);
    REQUIRE(text.find("failed, takes 10") != std::string::npos);
    REQUIRE(encounter.combatants()[1].current_hp <= 0);
    REQUIRE(encounter.status() == dnd::EncounterStatus::Victory);
}

TEST_CASE("reaction spells can't be cast on your own turn", "[reactions]") {
    Fight f;
    ScriptedRandomSource rolls({20, 1});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());
    encounter.add_character(f.hero("Wizard", 1), {f.spell("Shield")}, {});
    encounter.add_monsters(f.goblin(), 1);
    encounter.roll_initiative();
    REQUIRE(encounter.castable_spells().empty());
}
