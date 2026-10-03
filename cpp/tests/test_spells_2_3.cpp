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

struct Spells {
    dnd::Repository repo{":memory:"};

    dnd::Spell spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return {};
    }

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

    // A caster of `class_name` at `level` (casting ability 16), knowing `known`.
    dnd::Character caster(int id, const std::string& class_name, int level) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = class_name;
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == class_name) c.class_id = entry.id;
        }
        c.level = level;
        c.intelligence = 16;
        c.wisdom = 16;
        c.charisma = 16;
        c.max_hp = 40;
        c.current_hp = 40;
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

TEST_CASE("2nd- and 3rd-level spells are seeded with their classes", "[spells23]") {
    Spells s;
    auto fireball = s.spell("Fireball");
    REQUIRE(fireball.level == 3);
    REQUIRE(fireball.max_targets == 6);
    REQUIRE(fireball.save_halves);
    REQUIRE(std::find(fireball.class_names.begin(), fireball.class_names.end(), "Wizard") != fireball.class_names.end());
    auto hold = s.spell("Hold Person");
    REQUIRE(hold.concentration);
    REQUIRE(hold.target_creature_type == "Humanoid");
    REQUIRE(hold.effects.size() == 1);
    REQUIRE(hold.effects[0].repeat_save);
    REQUIRE(s.spell("Revivify").consumed_item == "Diamonds (300 gp)");
    REQUIRE(s.spell("Misty Step").resolution == dnd::SpellResolution::Utility);
    REQUIRE(s.item("Spell Scroll (Fireball)").cost_cp == 30000);
    REQUIRE(s.item("Spell Scroll (Shatter)").cost_cp == 15000);
}

TEST_CASE("Hold Person paralyzes, with a save at the end of each turn", "[spells23]") {
    Spells s;
    // Initiative: the wizard 20, the goblin 1. The goblin's WIS save: d20 2.
    ScriptedRandomSource rolls({20, 1, 2});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, s.repo.get_classes(), s.repo.get_races());
    auto wizard = s.caster(1, "Wizard", 3);  // DC 8 + 2 + 3 = 13
    encounter.add_character(wizard, {s.spell("Hold Person")}, {});
    encounter.add_monsters(s.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.cast_spell(s.spell("Hold Person").spell_id, 2, {1}));
    auto text = text_of(encounter.take_events());
    REQUIRE(text.find("vs DC 13 -> failed") != std::string::npos);
    auto held = std::find_if(encounter.effects().begin(), encounter.effects().end(), [](const dnd::ActiveEffect& e) {
        return e.target == 1 && e.kind == dnd::EffectKind::Condition && e.condition == dnd::Condition::Paralyzed;
    });
    REQUIRE(held != encounter.effects().end());
    REQUIRE(held->save_to_end.has_value());
    REQUIRE(held->save_to_end->dc == 13);
    REQUIRE(held->save_to_end->ability == dnd::Ability::Wisdom);
    REQUIRE(held->concentration);
}

TEST_CASE("Mass Healing Word heals each target", "[spells23]") {
    Spells s;
    // Initiative: the cleric 20, the fighter 19, the goblin 1; then 1d4 per target: 2, 3.
    ScriptedRandomSource rolls({20, 19, 1, 2, 3});
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, s.repo.get_classes(), s.repo.get_races());
    auto cleric = s.caster(1, "Cleric", 5);  // WIS +3
    cleric.current_hp = 10;
    auto fighter = s.caster(2, "Fighter", 5);
    fighter.current_hp = 10;
    encounter.add_character(cleric, {s.spell("Mass Healing Word")}, {});
    encounter.add_character(fighter, {}, {});
    encounter.add_monsters(s.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.cast_spell(s.spell("Mass Healing Word").spell_id, 3, {0, 1}));
    REQUIRE(encounter.combatants()[0].current_hp == 10 + 2 + 3);
    REQUIRE(encounter.combatants()[1].current_hp == 10 + 3 + 3);
}

TEST_CASE("Fireball hits every chosen target with one damage roll", "[spells23]") {
    Spells s;
    // Initiative 20 / 1 / 1 / 1; 8d6 all 3s = 24; each goblin's DEX save: 1, 1, 20.
    std::vector<int> script = {20, 1, 1, 1};
    for (int i = 0; i < 8; ++i) script.push_back(3);
    script.insert(script.end(), {1, 1, 20});
    ScriptedRandomSource rolls(script);
    dnd::Dice dice(rolls);
    dnd::Encounter encounter(dice, s.repo.get_classes(), s.repo.get_races());
    encounter.add_character(s.caster(1, "Wizard", 5), {s.spell("Fireball")}, {});
    encounter.add_monsters(s.monster("Ogre"), 3);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.cast_spell(s.spell("Fireball").spell_id, 3, {1, 2, 3}));
    REQUIRE(encounter.combatants()[1].current_hp == 59 - 24);
    REQUIRE(encounter.combatants()[2].current_hp == 59 - 24);
    REQUIRE(encounter.combatants()[3].current_hp == 59 - 12);  // saved: half
}

TEST_CASE("Revivify brings back someone who died in this fight, using up diamonds", "[spells23]") {
    Spells s;
    auto cleric = s.caster(1, "Cleric", 5);
    auto fighter = s.caster(2, "Fighter", 5);
    fighter.max_hp = 1;
    fighter.current_hp = 1;
    auto diamonds = s.item("Diamonds (300 gp)");

    SECTION("dead before the fight: too late") {
        ScriptedRandomSource rolls({20, 1, 1});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, s.repo.get_classes(), s.repo.get_races());
        fighter.dead = true;
        fighter.current_hp = 0;
        encounter.add_character(cleric, {s.spell("Revivify")}, {{diamonds, 1}});
        encounter.add_character(fighter, {}, {});
        encounter.add_monsters(s.monster("Goblin"), 1);
        encounter.roll_initiative();
        REQUIRE(encounter.spell_target_candidates(s.spell("Revivify")).empty());
    }
    SECTION("killed in this fight, then revived") {
        // Initiative: cleric 10, fighter 5, ogre 20. The ogre picks a target and
        // hits the fighter (1 HP) for at least 6: massive damage, dead.
        ScriptedRandomSource rolls({10, 5, 20, 2, 19, 4, 4});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, s.repo.get_classes(), s.repo.get_races());
        encounter.add_character(cleric, {s.spell("Revivify")}, {{diamonds, 1}});
        encounter.add_character(fighter, {}, {});
        encounter.add_monsters(s.monster("Ogre"), 1);
        encounter.roll_initiative();
        auto opening = text_of(encounter.take_events());
        INFO(opening);
        REQUIRE(encounter.combatants()[1].death_saves.dead);
        REQUIRE(encounter.current_index() == 0);  // the cleric's turn

        REQUIRE(encounter.spell_target_candidates(s.spell("Revivify")) == std::vector<int>{1});
        REQUIRE(encounter.cast_spell(s.spell("Revivify").spell_id, 3, {1}));
        auto events = encounter.take_events();
        REQUIRE(text_of(events).find("returns to life with 1 HP") != std::string::npos);
        REQUIRE_FALSE(encounter.combatants()[1].death_saves.dead);
        REQUIRE(encounter.combatants()[1].current_hp == 1);
        REQUIRE_FALSE(encounter.character(2)->dead);
        REQUIRE(std::any_of(events.begin(), events.end(), [&](const dnd::EncounterEvent& e) {
            return e.kind == dnd::EncounterEvent::Kind::ItemUsed && e.item_id == diamonds.item_id;
        }));
    }
    SECTION("no diamonds, no Revivify") {
        ScriptedRandomSource rolls({10, 5, 20, 2, 19, 4, 4});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, s.repo.get_classes(), s.repo.get_races());
        encounter.add_character(cleric, {s.spell("Revivify")}, {});
        encounter.add_character(fighter, {}, {});
        encounter.add_monsters(s.monster("Ogre"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE_FALSE(encounter.cast_spell(s.spell("Revivify").spell_id, 3, {1}));
        REQUIRE(text_of(encounter.take_events()).find("needs Diamonds (300 gp)") != std::string::npos);
        REQUIRE(encounter.character(1)->spell_slots_used[2] == 0);  // no slot spent
    }
}
