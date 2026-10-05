#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/encounter.hpp"
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

struct Lab {
    dnd::Repository repo{":memory:"};

    dnd::Item item(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i;
        }
        FAIL("no item " << name);
        return {};
    }

    dnd::Character fighter(int id = 1, const std::string& name = "Aria") {
        dnd::Character c;
        c.character_id = id;
        c.character_name = name;
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == "Fighter") c.class_id = entry.id;
        }
        c.level = 5;
        c.strength = 16;
        c.dexterity = 12;
        c.constitution = 12;
        c.max_hp = 40;
        c.current_hp = 40;
        return c;
    }

    // A plain CR 3 brute to dress up: AC 12, 60 HP, one longsword.
    dnd::Monster brute(const std::string& name) {
        dnd::Monster m;
        m.name = name;
        m.armor_class = 12;
        m.hit_points = 60;
        m.strength = 14;
        m.dexterity = 12;
        m.constitution = 14;
        m.wisdom = 10;
        m.challenge_rating = 3;
        m.creature_type = "Undead";
        dnd::MonsterAttack sword;
        sword.name = "Longsword";
        sword.attack_bonus = 4;
        sword.damage_dice_count = 1;
        sword.damage_dice_sides = 8;
        sword.damage_bonus = 2;
        sword.damage_type = "slashing";
        m.attacks = {sword};
        return m;
    }

    dnd::Encounter fight(dnd::Dice& dice) {
        dnd::Encounter encounter(dice, repo.get_classes(), repo.get_races());
        encounter.set_scroll_spells(repo.get_spells());
        return encounter;
    }
};

std::string text_of(const std::vector<dnd::EncounterEvent>& events) {
    std::string text;
    for (const auto& e : events) {
        if (e.kind == dnd::EncounterEvent::Kind::Log) text += e.text + "\n";
    }
    return text;
}

bool has(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("damage from nonmagical weapons", "[monster_abilities]") {
    Lab lab;
    REQUIRE(dnd::damage_modifier({{}, {}, {}, "resistant"}, "slashing", false) == dnd::DamageModifier::Resisted);
    REQUIRE(dnd::damage_modifier({{}, {}, {}, "resistant"}, "slashing", true) == dnd::DamageModifier::Normal);
    REQUIRE(dnd::damage_modifier({{}, {}, {}, "immune"}, "piercing", false) == dnd::DamageModifier::Immune);
    REQUIRE(dnd::damage_modifier({{}, {}, {}, "immune"}, "fire", false) == dnd::DamageModifier::Normal);

    auto hit = [&](const std::string& weapon, const std::string& traits) {
        // The fighter first; d20 15 hits, 1d10 5 (+3).
        ScriptedRandomSource rolls({20, 1, 15, 5});
        dnd::Dice dice(rolls);
        auto encounter = lab.fight(dice);
        encounter.add_character(lab.fighter(), {}, {{lab.item(weapon), 1, true}});
        auto wight = lab.brute("Wight");
        wight.damage.nonmagical_weapons = traits;
        encounter.add_monsters(wight, 1);
        encounter.roll_initiative();
        encounter.take_events();
        const auto& me = encounter.combatants()[0];
        dnd::AttackOptions none;
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus, none);
        return std::make_pair(60 - encounter.combatants()[1].current_hp, text_of(encounter.take_events()));
    };
    auto [plain, plain_log] = hit("Longsword", "resistant");
    REQUIRE(plain == 4);  // 8 halved
    REQUIRE(has(plain_log, "nonmagical"));
    REQUIRE(hit("Longsword +1", "resistant").first == 9);  // magic gets through (5 + 3 + 1)
    REQUIRE(hit("Longsword", "immune").first == 0);
}

TEST_CASE("monsters that prefer range shoot", "[monster_abilities]") {
    Lab lab;
    auto archer = lab.brute("Veteran");
    dnd::MonsterAttack bow;
    bow.name = "Heavy Crossbow";
    bow.attack_bonus = 3;
    bow.damage_dice_count = 1;
    bow.damage_dice_sides = 10;
    bow.damage_bonus = 1;
    bow.damage_type = "piercing";
    bow.ranged = true;
    bow.preferred = true;
    archer.attacks.push_back(bow);
    // A monk target shows which: Deflect Missiles only answers ranged attacks.
    auto monk = lab.fighter();
    for (const auto& entry : lab.repo.get_classes()) {
        if (entry.name == "Monk") monk.class_id = entry.id;
    }
    ScriptedRandomSource rolls({1, 20, 1, 19, 5, 3});
    dnd::Dice dice(rolls);
    auto encounter = lab.fight(dice);
    encounter.add_character(monk, {}, {});
    encounter.add_monsters(archer, 1);
    encounter.roll_initiative();
    auto text = text_of(encounter.take_events());
    INFO(text);
    REQUIRE(has(text, "Veteran attacks Aria"));
    REQUIRE(has(text, "Deflect Missiles"));
}

TEST_CASE("Life Drain lowers max HP until a long rest", "[monster_abilities]") {
    Lab lab;
    auto wight = lab.brute("Wight");
    wight.attacks.front().name = "Life Drain";
    wight.attacks.front().damage_type = "necrotic";
    wight.attacks.front().life_drain_dc = 13;
    // The wight first: d20 18 hits, 1d8 6 (+2) = 8 necrotic; Aria's CON save d20 2 fails.
    ScriptedRandomSource rolls({1, 20, 1, 18, 6, 2});
    dnd::Dice dice(rolls);
    auto encounter = lab.fight(dice);
    encounter.add_character(lab.fighter(), {}, {});
    encounter.add_monsters(wight, 1);
    encounter.roll_initiative();
    auto text = text_of(encounter.take_events());
    INFO(text);
    REQUIRE(has(text, "Life Drain: Aria"));
    REQUIRE(has(text, "max HP drops by 8 until a long rest"));
    auto aria = *encounter.character(1);
    REQUIRE(aria.max_hp == 32);
    REQUIRE(aria.max_hp_reduction == 8);
    dnd::take_long_rest(aria);
    REQUIRE(aria.max_hp == 40);
    REQUIRE(aria.current_hp == 40);
    REQUIRE(aria.max_hp_reduction == 0);
}

TEST_CASE("breath weapons and recharge", "[monster_abilities]") {
    Lab lab;
    auto dragon = lab.brute("White Dragon");
    dragon.creature_type = "Dragon";
    dragon.breath = dnd::MonsterBreath{"Cold Breath", 4, 6, "cold", dnd::Ability::Constitution, 13, 5, 4, false};

    SECTION("it breathes when ready, half on a save, then needs a 5-6 to recharge") {
        // Initiative: Aria 1, Bram 1, dragon 20. Breath: targets, 4d6 = 4 x 4 = 16; saves: 2 (fail), 20 (half).
        ScriptedRandomSource rolls({1, 1, 20, 1, 1, 4, 4, 4, 4, 2, 20, 3});
        dnd::Dice dice(rolls);
        auto encounter = lab.fight(dice);
        encounter.add_character(lab.fighter(1, "Aria"), {}, {});
        encounter.add_character(lab.fighter(2, "Bram"), {}, {});
        encounter.add_monsters(dragon, 1);
        encounter.roll_initiative();
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "White Dragon uses Cold Breath! (16 cold)"));
        REQUIRE(encounter.combatants()[2].breath_ready == false);
        int lost = (40 - encounter.combatants()[0].current_hp) + (40 - encounter.combatants()[1].current_hp);
        REQUIRE(lost == 16 + 8);
    }
    SECTION("the Gorgon's breath restrains, then petrifies on a second failure") {
        auto gorgon = lab.brute("Gorgon");
        gorgon.breath = dnd::MonsterBreath{"Petrifying Breath", 0, 8, "", dnd::Ability::Constitution, 13, 5, 4, true};
        ScriptedRandomSource rolls({1, 20, 1, 1});  // every save fails
        dnd::Dice dice(rolls);
        auto encounter = lab.fight(dice);
        encounter.add_character(lab.fighter(), {}, {});
        encounter.add_monsters(gorgon, 1);
        encounter.roll_initiative();
        REQUIRE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Restrained));
        encounter.take_events();
        encounter.dodge();
        encounter.next_turn();  // Aria's turn ends: the second save fails
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "turns to stone"));
        REQUIRE(encounter.status() == dnd::EncounterStatus::Defeat);  // the only character is stone
    }
}

TEST_CASE("monster spellcasting", "[monster_abilities]") {
    Lab lab;
    auto mage = lab.brute("Mage");
    mage.creature_type = "Humanoid";
    mage.spellcasting = dnd::MonsterSpellcasting{9, 14, 6, 3, {4, 3, 3}, {"Fire Bolt", "Magic Missile", "Shield", "Fireball"}};

    SECTION("a Fireball at two characters") {
        ScriptedRandomSource rolls({1, 1, 20, 3});
        dnd::Dice dice(rolls);
        auto encounter = lab.fight(dice);
        encounter.add_character(lab.fighter(1, "Aria"), {}, {});
        encounter.add_character(lab.fighter(2, "Bram"), {}, {});
        encounter.add_monsters(mage, 1);
        encounter.roll_initiative();
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "Mage casts Fireball (3rd-level slot)."));
        REQUIRE(encounter.combatants()[2].spellcasting->spell_slots[2] == 2);
    }
    SECTION("Hold Person on a lone fighter") {
        auto fanatic = lab.brute("Cult Fanatic");
        fanatic.creature_type = "Humanoid";
        fanatic.spellcasting = dnd::MonsterSpellcasting{4, 11, 3, 2, {4, 3}, {"Sacred Flame", "Hold Person"}};
        ScriptedRandomSource rolls({1, 20, 1});
        dnd::Dice dice(rolls);
        auto encounter = lab.fight(dice);
        encounter.add_character(lab.fighter(), {}, {});
        encounter.add_monsters(fanatic, 1);
        encounter.roll_initiative();
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "Cult Fanatic casts Hold Person (2nd-level slot)."));
        REQUIRE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Paralyzed));
    }
    SECTION("the mage's Shield turns a hit into a miss") {
        // Aria first: d20 10 + 6 = 16 vs AC 12 hits; Shield makes it 17.
        ScriptedRandomSource rolls({20, 1, 10, 5});
        dnd::Dice dice(rolls);
        auto encounter = lab.fight(dice);
        encounter.add_character(lab.fighter(), {}, {{lab.item("Longsword"), 1, true}});
        encounter.add_monsters(mage, 1);
        encounter.roll_initiative();
        encounter.take_events();
        const auto& me = encounter.combatants()[0];
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "Mage casts Shield as a reaction: AC 17, and the attack misses."));
        REQUIRE(encounter.combatants()[1].current_hp == 60);
    }
}
