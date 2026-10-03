#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/character_io.hpp"
#include "dnd/encounter.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"
#include "dnd/spellbook.hpp"
#include "dnd/spellcasting.hpp"

namespace {

struct Casters {
    dnd::Repository repo{":memory:"};
    int player = repo.add_player("Adam");

    dnd::Character add(const std::string& class_name, int level) {
        dnd::Character c;
        c.character_name = class_name;
        c.player_id = player;
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == class_name) c.class_id = entry.id;
        }
        c.race = repo.get_races().front().id;
        c.alignment = repo.get_alignments().front().id;
        c.level = level;
        c.intelligence = 16;
        c.wisdom = 16;
        c.max_hp = 20;
        c.current_hp = 20;
        c.character_id = repo.add_character(c);
        return c;
    }

    int spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s.spell_id;
        }
        FAIL("no spell " << name);
        return 0;
    }

    dnd::Character get(int id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == id) return c;
        }
        return {};
    }

    std::vector<std::string> prepared(int id) {
        std::vector<std::string> names;
        for (const auto& s : repo.get_character_spells(id)) {
            if (s.prepared && s.level > 0) names.push_back(s.name);
        }
        return names;
    }
};

const char* kSixSpells[] = {"Magic Missile", "Burning Hands", "Thunderwave", "Mage Armor", "Detect Magic",
                            "Feather Fall"};

}  // namespace

TEST_CASE("a Wizard's spellbook: six spells, INT + level prepared", "[preparing]") {
    Casters t;
    auto wizard = t.add("Wizard", 1);  // INT +3: 4 prepared
    const auto& info = *dnd::find_class_info("Wizard");
    for (const char* name : kSixSpells) dnd::add_known_spell(t.repo, wizard, info, t.spell(name));
    REQUIRE(t.repo.get_character_spells(wizard.character_id).size() == 6);
    REQUIRE(t.prepared(wizard.character_id) ==
            std::vector<std::string>{"Burning Hands", "Mage Armor", "Magic Missile", "Thunderwave"});

    SECTION("swapping while the window is open") {
        auto full = dnd::set_prepared(t.repo, wizard, info, t.spell("Feather Fall"), true);
        REQUIRE_FALSE(full.ok);
        REQUIRE(full.message == "Already 4 spells prepared: unprepare one first.");
        REQUIRE(dnd::set_prepared(t.repo, wizard, info, t.spell("Thunderwave"), false).ok);
        REQUIRE(dnd::set_prepared(t.repo, wizard, info, t.spell("Feather Fall"), true).ok);
        auto names = t.prepared(wizard.character_id);
        REQUIRE(std::find(names.begin(), names.end(), "Feather Fall") != names.end());
    }
    SECTION("the window closes when a fight starts, and opens on a long rest") {
        dnd::SystemRandomSource source;
        dnd::Dice dice(source);
        dnd::Encounter encounter(dice, t.repo.get_classes(), t.repo.get_races());
        encounter.add_character(wizard, t.repo.get_character_spells(wizard.character_id), {});
        for (const auto& m : t.repo.get_monsters()) {
            if (m.name == "Goblin") encounter.add_monsters(m, 1);
        }
        encounter.roll_initiative();
        auto fought = *encounter.character(wizard.character_id);
        REQUIRE_FALSE(fought.may_prepare);
        auto refused = dnd::set_prepared(t.repo, fought, info, t.spell("Feather Fall"), true);
        REQUIRE_FALSE(refused.ok);
        REQUIRE(refused.message == "Prepared spells change only after a long rest (and before the next fight).");

        dnd::take_long_rest(fought);
        REQUIRE(fought.may_prepare);
    }
    SECTION("combat offers only prepared spells") {
        dnd::SystemRandomSource source;
        dnd::Dice dice(source);
        dnd::Encounter encounter(dice, t.repo.get_classes(), t.repo.get_races());
        encounter.add_character(wizard, t.repo.get_character_spells(wizard.character_id), {});
        for (const auto& m : t.repo.get_monsters()) {
            if (m.name == "Goblin") encounter.add_monsters(m, 1);
        }
        while (encounter.current_index() != 0 && encounter.status() == dnd::EncounterStatus::InProgress) {
            encounter.roll_initiative();
        }
        if (encounter.current_index() == 0) {
            auto castable = encounter.castable_spells();
            bool feather = std::any_of(castable.begin(), castable.end(),
                                       [](const dnd::Spell& s) { return s.name == "Feather Fall"; });
            REQUIRE_FALSE(feather);  // in the book, not prepared
            REQUIRE(castable.size() == 4);
        }
    }
    SECTION("exports keep what's prepared") {
        auto file = dnd::export_character(t.repo, wizard.character_id);
        auto parsed = dnd::parse_character_file(dnd::to_json(file));
        REQUIRE(parsed.file->characters.at(0).unprepared_spells ==
                std::vector<std::string>{"Detect Magic", "Feather Fall"});
    }
}

TEST_CASE("only Wizards prepare from a book; others' allowances", "[preparing]") {
    Casters t;
    auto cleric = t.add("Cleric", 1);
    auto refused = dnd::set_prepared(t.repo, cleric, *dnd::find_class_info("Cleric"), t.spell("Bless"), true);
    REQUIRE_FALSE(refused.ok);
    REQUIRE(dnd::prepares_spells(*dnd::find_class_info("Cleric")));
    REQUIRE_FALSE(dnd::prepares_spells(*dnd::find_class_info("Bard")));
    REQUIRE(dnd::spell_allowance(t.get(cleric.character_id), *dnd::find_class_info("Cleric")).spells == 4);

    auto wizard = t.add("Wizard", 3);
    REQUIRE(dnd::spell_allowance(wizard, *dnd::find_class_info("Wizard")).spells == 10);  // 6 + 2 + 2
    REQUIRE(dnd::prepared_spell_count(wizard, *dnd::find_class_info("Wizard")) == 6);
}
