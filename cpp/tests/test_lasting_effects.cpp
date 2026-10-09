#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/character_io.hpp"
#include "dnd/encounter.hpp"
#include "dnd/equipment.hpp"
#include "dnd/lasting_effects.hpp"
#include "dnd/progression.hpp"
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

struct Fixture {
    dnd::Repository repo{":memory:"};

    dnd::Spell spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return {};
    }

    dnd::Item item(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i;
        }
        FAIL("no item " << name);
        return {};
    }

    dnd::Monster goblin() {
        for (const auto& m : repo.get_monsters()) {
            if (m.name == "Goblin") return m;
        }
        FAIL("no goblin");
        return {};
    }

    int class_id(const std::string& name) {
        for (const auto& c : repo.get_classes()) {
            if (c.name == name) return c.id;
        }
        return 0;
    }

    // A level-1 fighter in chain mail (AC 16).
    dnd::Character fighter() {
        dnd::Character c;
        c.character_id = 1;
        c.character_name = "Aria";
        c.class_id = class_id("Fighter");
        c.strength = 16;
        c.dexterity = 12;
        c.max_hp = 12;
        c.current_hp = 12;
        return c;
    }
    std::vector<dnd::InventoryEntry> chain_mail() { return {{item("Chain Mail"), 1, true}}; }

    // A level-3 cleric with DEX 14 and no armor (AC 12): 2nd-level slots for Aid.
    dnd::Character cleric() {
        dnd::Character c;
        c.character_id = 2;
        c.character_name = "Zanna";
        c.class_id = class_id("Cleric");
        c.level = 3;
        c.wisdom = 16;
        c.dexterity = 14;
        c.max_hp = 20;
        c.current_hp = 20;
        return c;
    }
};

std::vector<std::string> logs(const std::vector<dnd::EncounterEvent>& events) {
    std::vector<std::string> lines;
    for (const auto& e : events) {
        if (e.kind == dnd::EncounterEvent::Kind::Log) lines.push_back(e.text);
    }
    return lines;
}

bool contains(const std::vector<std::string>& lines, const std::string& needle) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& line) { return line.find(needle) != std::string::npos; });
}

}  // namespace

TEST_CASE("Mage Armor: unarmored AC 13 + DEX until a long rest", "[lasting]") {
    Fixture f;
    auto mage_armor = f.spell("Mage Armor");
    REQUIRE(mage_armor.resolution == dnd::SpellResolution::Lasting);
    auto zanna = f.cleric();
    REQUIRE(dnd::calculate_armor_class(zanna, "Cleric", {}).total == 12);

    REQUIRE(dnd::lasting_refusal(mage_armor, zanna, {}, 1).empty());
    REQUIRE(dnd::apply_lasting_spell(mage_armor, zanna, 1) == "Mage Armor until a long rest");
    auto ac = dnd::calculate_armor_class(zanna, "Cleric", {});
    REQUIRE(ac.total == 15);
    REQUIRE(ac.parts.front().label == "Mage Armor");
    REQUIRE(dnd::lasting_refusal(mage_armor, zanna, {}, 1) == "Zanna already has Mage Armor.");

    SECTION("not on someone wearing armor, and putting armor on ends it") {
        auto aria = f.fighter();
        REQUIRE(dnd::lasting_refusal(mage_armor, aria, f.chain_mail(), 1) == "Aria is wearing armor.");
        REQUIRE(dnd::calculate_armor_class(zanna, "Cleric", f.chain_mail()).total == 16);  // chain mail wins
        REQUIRE(dnd::end_mage_armor_if_armored(zanna, f.chain_mail()));
        REQUIRE_FALSE(dnd::has_lasting_effect(zanna, dnd::kMageArmor));
        REQUIRE_FALSE(dnd::end_mage_armor_if_armored(zanna, f.chain_mail()));
    }

    SECTION("a better unarmored base still wins") {
        dnd::Character monk = zanna;
        monk.wisdom = 18;  // Unarmored Defense: 10 + 2 + 4 = 16 beats 15
        REQUIRE(dnd::calculate_armor_class(monk, "Monk", {}).total == 16);
    }

    SECTION("a long rest ends it") {
        dnd::take_long_rest(zanna);
        REQUIRE(zanna.lasting_effects.empty());
        REQUIRE(dnd::calculate_armor_class(zanna, "Cleric", {}).total == 12);
    }
}

TEST_CASE("Aid: +5 max and current HP per slot level above 1st, no stacking", "[lasting]") {
    Fixture f;
    auto aid = f.spell("Aid");
    REQUIRE(aid.max_targets == 3);
    REQUIRE(dnd::aid_amount(2) == 5);
    REQUIRE(dnd::aid_amount(4) == 15);

    auto aria = f.fighter();
    aria.current_hp = 7;
    REQUIRE(dnd::apply_lasting_spell(aid, aria, 2) == "Aid: +5 max HP");
    REQUIRE(aria.max_hp == 17);
    REQUIRE(aria.current_hp == 12);
    REQUIRE(dnd::describe_lasting_effects(aria) == "Aid (+5 HP)");

    SECTION("the same slot again does nothing; a stronger one replaces it") {
        REQUIRE(dnd::lasting_refusal(aid, aria, {}, 2) == "Aria already has Aid.");
        REQUIRE(dnd::lasting_refusal(aid, aria, {}, 3).empty());
        REQUIRE(dnd::apply_lasting_spell(aid, aria, 3) == "Aid: +5 max HP");
        REQUIRE(aria.max_hp == 22);
        REQUIRE(dnd::aid_bonus(aria) == 10);
    }

    SECTION("a long rest takes it back off, with Life Drain's loss restored") {
        aria.max_hp -= 3;
        aria.max_hp_reduction = 3;
        dnd::take_long_rest(aria);
        REQUIRE(aria.max_hp == 12);
        REQUIRE(aria.current_hp == 12);
        REQUIRE(aria.lasting_effects.empty());
    }

    SECTION("not on the dead") {
        aria.dead = true;
        REQUIRE(dnd::lasting_refusal(aid, aria, {}, 3) == "Aria is dead.");
    }
}

TEST_CASE("lasting effects cast in combat stay on the character", "[lasting][encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({10, 20, 1});  // initiative: Zanna, Aria, the Goblin
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, f.repo.get_classes(), f.repo.get_races());
    encounter.add_character(f.fighter(), {}, f.chain_mail());
    encounter.add_character(f.cleric(), {f.spell("Mage Armor"), f.spell("Aid")}, {});
    encounter.add_monsters(f.goblin(), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current_index() == 1);

    SECTION("Mage Armor: not on Aria in chain mail; Zanna's AC goes up now and later") {
        auto mage_armor = f.spell("Mage Armor");
        REQUIRE(encounter.spell_target_candidates(mage_armor) == std::vector<int>{1});
        REQUIRE(encounter.cast_spell(mage_armor.spell_id, 1, {1}));
        auto events = encounter.take_events();
        REQUIRE(contains(logs(events), "    Zanna: Mage Armor until a long rest (AC 15)."));
        REQUIRE(encounter.armor_class(1) == 15);
        REQUIRE(dnd::has_lasting_effect(*encounter.character(2), dnd::kMageArmor));
        REQUIRE(std::any_of(events.begin(), events.end(), [](const dnd::EncounterEvent& e) {
            return e.kind == dnd::EncounterEvent::Kind::CharacterChanged && e.character_id == 2;
        }));
    }

    SECTION("Aid on both: max and current HP rise in the fight and on the characters") {
        auto aid = f.spell("Aid");
        REQUIRE(encounter.cast_spell(aid.spell_id, 2, {0, 1}));
        auto lines = logs(encounter.take_events());
        REQUIRE(contains(lines, "    Aria: Aid: +5 max HP. Now 17/17 HP."));
        REQUIRE(contains(lines, "    Zanna: Aid: +5 max HP. Now 25/25 HP."));
        REQUIRE(encounter.combatants()[0].max_hp == 17);
        REQUIRE(encounter.character(1)->max_hp == 17);
        REQUIRE(encounter.character(1)->current_hp == 17);
        REQUIRE(dnd::aid_bonus(*encounter.character(2)) == 5);
        REQUIRE(encounter.spell_target_candidates(aid) == std::vector<int>{0, 1});  // a 3rd-level Aid still could
    }
}

TEST_CASE("lasting effects are saved and exported", "[lasting][repository]") {
    REQUIRE(dnd::parse_lasting_effects(dnd::format_lasting_effects({{"Aid", 10}, {"Mage Armor", 0}})) ==
            std::map<std::string, int>{{"Aid", 10}, {"Mage Armor", 0}});
    REQUIRE(dnd::parse_lasting_effects("").empty());

    dnd::Repository repo(":memory:");
    int player = repo.add_player("Ann");
    dnd::Character c;
    c.character_name = "Zanna";
    c.class_id = 1;
    c.race = 1;
    c.alignment = 1;
    c.player_id = player;
    c.lasting_effects = {{"Mage Armor", 0}};
    c.character_id = repo.add_character(c);
    REQUIRE(repo.get_characters().front().lasting_effects == c.lasting_effects);

    c.lasting_effects[dnd::kAid] = 5;
    repo.update_character(c);
    REQUIRE(repo.get_characters().front().lasting_effects == c.lasting_effects);

    auto parsed = dnd::parse_character_file(dnd::to_json(dnd::export_character(repo, c.character_id)));
    REQUIRE(parsed.file);
    REQUIRE(parsed.file->characters.front().lasting_effects == c.lasting_effects);
}
