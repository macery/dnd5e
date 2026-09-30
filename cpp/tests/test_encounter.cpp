#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/encounter.hpp"
#include "dnd/repository.hpp"

namespace {

using dnd::EncounterEvent;

// Returns the scripted values in order, then repeats the last one forever --
// so a test only scripts the rolls it cares about.
class ScriptedRandomSource : public dnd::IRandomSource {
public:
    explicit ScriptedRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int sides) override {
        int value = index_ < values_.size() ? values_[index_++] : values_.back();
        return std::min(value, sides);  // e.g. a "pick 1 of 1 targets" roll
    }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

// Seeded data from an in-memory repository, so fights use real stat blocks.
struct Fixture {
    dnd::Repository repo{":memory:"};

    const dnd::Monster& monster(const std::string& name) {
        static std::vector<dnd::Monster> monsters;
        monsters = repo.get_monsters();
        for (const auto& m : monsters) {
            if (m.name == name) return m;
        }
        FAIL("no monster " << name);
        return monsters.front();
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

    int class_id(const std::string& name) {
        for (const auto& c : repo.get_classes()) {
            if (c.name == name) return c.id;
        }
        return 0;
    }

    int weapon_id(const std::string& name) {
        for (const auto& w : repo.get_weapons()) {
            if (w.name == name) return w.weapon_id;
        }
        return 0;
    }

    // A level-1 fighter with a longsword: +5 to hit, 1d8+3.
    dnd::Character fighter(int id = 1) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = "Aria";
        c.class_id = class_id("Fighter");
        c.strength = 16;
        c.dexterity = 12;
        c.constitution = 14;
        c.max_hp = 12;
        c.current_hp = 12;
        c.armor_class = 16;
        c.weapon_id = weapon_id("Longsword");
        return c;
    }

    // A level-1 wizard: INT 16, spell attack +5, save DC 13, two 1st-level slots.
    dnd::Character wizard(int id = 2) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = "Zanna";
        c.class_id = class_id("Wizard");
        c.intelligence = 16;
        c.dexterity = 14;
        c.max_hp = 8;
        c.current_hp = 8;
        c.armor_class = 12;
        return c;
    }

    dnd::Encounter encounter(const dnd::Dice& dice) {
        return dnd::Encounter(dice, repo.get_classes(), repo.get_weapons());
    }
};

std::vector<std::string> logs(const std::vector<EncounterEvent>& events) {
    std::vector<std::string> lines;
    for (const auto& e : events) {
        if (e.kind == EncounterEvent::Kind::Log) lines.push_back(e.text);
    }
    return lines;
}

bool has_event(const std::vector<EncounterEvent>& events, EncounterEvent::Kind kind, int character_id,
               int item_id = 0) {
    return std::any_of(events.begin(), events.end(), [&](const EncounterEvent& e) {
        return e.kind == kind && e.character_id == character_id && (item_id == 0 || e.item_id == item_id);
    });
}

bool contains(const std::vector<std::string>& lines, const std::string& needle) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& line) { return line.find(needle) != std::string::npos; });
}

}  // namespace

TEST_CASE("setup builds combatants from characters and monsters", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({10});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 2);

    const auto& combatants = encounter.combatants();
    REQUIRE(combatants.size() == 3);
    REQUIRE(combatants[0].name == "Aria");
    REQUIRE(combatants[0].attack_bonus == 5);  // +2 proficiency, +3 STR
    REQUIRE(combatants[0].damage_dice_sides == 8);
    REQUIRE(combatants[0].damage_bonus == 3);
    REQUIRE(combatants[1].name == "Goblin #1");
    REQUIRE(combatants[2].name == "Goblin #2");
    REQUIRE(combatants[1].xp == 50);
    REQUIRE(combatants[1].ability_scores[1] == 14);  // DEX

    REQUIRE_FALSE(encounter.started());
    REQUIRE(encounter.current() == nullptr);
    REQUIRE_FALSE(encounter.awaiting_action());
    REQUIRE_FALSE(encounter.dash());  // no turn yet
}

TEST_CASE("initiative order, then monsters act until a character's turn", "[encounter]") {
    Fixture f;
    // Initiative d20s in combatant order: Aria 5 (+1 = 6), Goblin 15 (+2 = 17).
    // Goblin's turn: target pick (1 of 1), attack d20 3 (+4 = 7 vs AC 16: miss).
    ScriptedRandomSource rolls({5, 15, 1, 3});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);

    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(lines.at(0) == "--- Initiative rolled. Combat begins! ---");
    REQUIRE(lines.at(1) == "Goblin attacks Aria: d20(3) +4 -> miss. Aria is now 12/12 HP.");

    REQUIRE(encounter.combatants()[0].initiative == 6);
    REQUIRE(encounter.combatants()[1].initiative == 17);
    REQUIRE(encounter.current_index() == 0);
    REQUIRE(encounter.awaiting_action());
    REQUIRE(encounter.attack_targets() == std::vector<int>{1});
}

TEST_CASE("a character gets one action per turn", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1, 1, 1});  // Aria first; goblin misses on 1s
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.dash());
    REQUIRE(logs(encounter.take_events()) == std::vector<std::string>{"Aria takes the Dash action."});
    REQUIRE_FALSE(encounter.awaiting_action());
    REQUIRE_FALSE(encounter.dodge());
    REQUIRE_FALSE(encounter.attack(1, 5, 1, 8, 3));

    encounter.next_turn();  // goblin acts, back to Aria
    REQUIRE(encounter.current_index() == 0);
    REQUIRE(encounter.awaiting_action());
}

TEST_CASE("dodge gives monsters disadvantage until the dodger's next turn", "[encounter]") {
    Fixture f;
    // Init: Aria 20, Goblin 1. Aria dodges. Goblin: pick, then 2d20 (18, 2):
    // disadvantage takes the 2 -> miss even though 18 would hit.
    ScriptedRandomSource rolls({20, 1, 1, 18, 2});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.dodge());
    REQUIRE(encounter.combatants()[0].is_dodging);
    encounter.next_turn();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Goblin attacks Aria: d20(2) +4 -> miss."));
    REQUIRE_FALSE(encounter.combatants()[0].is_dodging);  // cleared at the start of Aria's turn
}

TEST_CASE("attacking to victory awards XP and loot", "[encounter]") {
    Fixture f;
    // Init: Aria 20, Goblin 1. Aria's attack: d20 15 (+5 = 20 hit), 1d8 = 8 (+3 = 11 >= 7 HP).
    ScriptedRandomSource rolls({20, 1, 15, 8});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.set_victory_reward("The trail is clear.", {{f.item("Potion of Healing"), 2}});
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.attack(1, 5, 1, 8, 3));
    auto events = encounter.take_events();
    auto lines = logs(events);
    REQUIRE(lines.at(0) == "Aria attacks Goblin: d20(15) +5 -> hit for 11. Goblin is now 0/7 HP.");
    REQUIRE(lines.at(1) == "--- Victory! All enemies defeated. The trail is clear. ---");
    REQUIRE(lines.at(2) == "The party earns 50 XP (50 each).");
    REQUIRE(lines.at(3) == "Loot! Aria takes: 2 × Potion of Healing.");

    REQUIRE(encounter.status() == dnd::EncounterStatus::Victory);
    REQUIRE(encounter.result_message() == "Victory! All enemies defeated. The trail is clear.");
    REQUIRE(encounter.character(1)->experience == 50);
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
    REQUIRE(has_event(events, EncounterEvent::Kind::ItemGained, 1, f.item("Potion of Healing").item_id));

    REQUIRE(encounter.current() == nullptr);
    REQUIRE_FALSE(encounter.dash());
    encounter.next_turn();  // no-op once over
    REQUIRE(encounter.take_events().empty());
}

TEST_CASE("victory can level a character up", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1, 15, 8});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.experience = 280;  // 300 is level 2
    encounter.add_character(aria, {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.attack(1, 5, 1, 8, 3);

    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "*** Aria reaches level 2! Max HP is now 20. ***"));  // d10 avg 6 + CON 2
    REQUIRE(encounter.character(1)->level == 2);
    REQUIRE(encounter.combatants()[0].max_hp == 20);
}

TEST_CASE("defeat when every character is down", "[encounter]") {
    Fixture f;
    // Init: Aria 1, Orc 20. Orc: pick, d20 20 (crit), 2d12 = 12 + 12 + 3 = 27.
    ScriptedRandomSource rolls({1, 20, 1, 20, 12, 12});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();

    auto events = encounter.take_events();
    auto lines = logs(events);
    REQUIRE(contains(lines, "Orc attacks Aria: d20(20) +5 -> CRITICAL HIT for 27. Aria is now 0/12 HP."));
    REQUIRE(lines.back() == "--- Defeat! All characters have fallen. ---");
    REQUIRE(encounter.status() == dnd::EncounterStatus::Defeat);
    REQUIRE(encounter.character(1)->current_hp == 0);
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
}

TEST_CASE("next_turn skips combatants who are down", "[encounter]") {
    Fixture f;
    // Init: Aria 20, Goblin #1 10, Goblin #2 5. Aria kills #1 (d20 15, 1d8 8).
    // Next turn: #1 is skipped; #2 acts (pick, d20 1 miss); back to Aria.
    ScriptedRandomSource rolls({20, 10, 5, 15, 8, 1, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 2);
    encounter.roll_initiative();
    REQUIRE(encounter.attack(1, 5, 1, 8, 3));
    encounter.take_events();

    encounter.next_turn();
    auto lines = logs(encounter.take_events());
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].rfind("Goblin #2 attacks Aria", 0) == 0);
    REQUIRE(encounter.current_index() == 0);
    REQUIRE(encounter.attack_targets() == std::vector<int>{2});
}

TEST_CASE("drinking a potion heals and uses it up", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1, 3, 3});  // Aria first; potion 2d4 = 3 + 3 (+2)
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 2;
    auto potion = f.item("Potion of Healing");
    encounter.add_character(aria, {}, {{potion, 1}, {f.item("Torch"), 5}});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    auto potions = encounter.healing_items();
    REQUIRE(potions.size() == 1);  // the torch isn't a healing item
    REQUIRE(potions[0].quantity == 1);

    REQUIRE_FALSE(encounter.drink_potion(f.item("Torch").item_id));
    encounter.take_events();  // the "no healing potions" line
    REQUIRE(encounter.drink_potion(potion.item_id));
    auto events = encounter.take_events();
    REQUIRE(logs(events).at(0) == "Aria drinks a Potion of Healing and regains 8 HP (now 10/12).");
    REQUIRE(has_event(events, EncounterEvent::Kind::ItemUsed, 1, potion.item_id));
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
    REQUIRE(encounter.character(1)->current_hp == 10);
    REQUIRE(encounter.healing_items().empty());
}

TEST_CASE("an invalid choice doesn't use up the action", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE_FALSE(encounter.attack(0, 5, 1, 8, 3));  // can't attack yourself
    REQUIRE_FALSE(encounter.drink_potion(1));         // no potions
    REQUIRE(logs(encounter.take_events()) ==
            std::vector<std::string>{"Aria has no healing potions -- choose another action."});
    REQUIRE_FALSE(encounter.cast_spell(1, 0, {1}));  // knows no spells
    REQUIRE(encounter.awaiting_action());
    REQUIRE(encounter.dash());
}

TEST_CASE("casting spells: slots, targets, damage, and healing", "[encounter]") {
    Fixture f;
    auto missile = f.spell("Magic Missile");
    auto burning = f.spell("Burning Hands");
    auto fire_bolt = f.spell("Fire Bolt");
    auto light = f.spell("Light");

    // Init: Zanna 20, Goblin #1 1, Goblin #2 1. Every later d4/d6 roll is 1.
    ScriptedRandomSource rolls({20, 1, 1, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.wizard(), {fire_bolt, light, missile, burning}, {});
    encounter.add_monsters(f.monster("Goblin"), 2);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current_index() == 0);

    REQUIRE(encounter.castable_spells().size() == 4);
    REQUIRE(encounter.spell_slot_options(missile) == std::vector<int>{1});
    REQUIRE(encounter.spell_slot_options(fire_bolt).empty());
    REQUIRE(encounter.spell_target_candidates(missile) == std::vector<int>{1, 2});

    SECTION("invalid target counts leave the slot and action unused") {
        REQUIRE_FALSE(encounter.cast_spell(burning.spell_id, 1, {}));
        REQUIRE_FALSE(encounter.cast_spell(burning.spell_id, 1, {0}));  // not a valid target
        REQUIRE_FALSE(encounter.cast_spell(burning.spell_id, 2, {1}));  // no 2nd-level slot
        REQUIRE(encounter.character(2)->spell_slots_used[0] == 0);
        REQUIRE(encounter.awaiting_action());
    }

    SECTION("Magic Missile spends a slot and splits darts across targets") {
        REQUIRE(encounter.cast_spell(missile.spell_id, 1, {1, 2}));
        auto events = encounter.take_events();
        auto lines = logs(events);
        REQUIRE(lines.at(0) == "Zanna casts Magic Missile (1st-level slot).");
        REQUIRE(lines.at(1) == "    Goblin #1: hit for 2. Now 5/7 HP.");
        REQUIRE(lines.at(2) == "    Goblin #2: hit for 2. Now 5/7 HP.");
        REQUIRE(lines.at(3) == "    Goblin #1: hit for 2. Now 3/7 HP.");
        REQUIRE(encounter.character(2)->spell_slots_used[0] == 1);
        REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 2));
        REQUIRE_FALSE(encounter.awaiting_action());
    }

    SECTION("a Save spell logs the shared damage roll and each save") {
        REQUIRE(encounter.cast_spell(burning.spell_id, 1, {1}));
        auto lines = logs(encounter.take_events());
        REQUIRE(lines.at(0) == "Zanna casts Burning Hands (1st-level slot).");
        REQUIRE(lines.at(1) == "    Damage roll: 1 + 1 + 1");
        REQUIRE(lines.at(2) == "    Goblin #1: DEX save d20(1) +2 vs DC 13 -> failed, takes 3. Now 4/7 HP.");
    }

    SECTION("a Utility spell just logs its description") {
        REQUIRE(encounter.cast_spell(light.spell_id, 0, {}));
        auto lines = logs(encounter.take_events());
        REQUIRE(lines.at(0) == "Zanna casts Light.");
        REQUIRE(lines.at(1) == "    " + light.description);
    }

    SECTION("with both slots spent, only cantrips remain castable") {
        auto zanna = f.wizard();
        zanna.spell_slots_used[0] = 2;
        ScriptedRandomSource rolls2({20, 1});
        dnd::Dice dice2(rolls2);
        auto spent = f.encounter(dice2);
        spent.add_character(zanna, {fire_bolt, missile}, {});
        spent.add_monsters(f.monster("Goblin"), 1);
        spent.roll_initiative();
        auto castable = spent.castable_spells();
        REQUIRE(castable.size() == 1);
        REQUIRE(castable[0].name == "Fire Bolt");
    }
}

TEST_CASE("healing spells target the party, including anyone down", "[encounter]") {
    Fixture f;
    auto cure = f.spell("Cure Wounds");
    auto cleric = f.wizard(3);
    cleric.character_name = "Brother Tam";
    cleric.class_id = f.class_id("Cleric");
    cleric.wisdom = 16;  // +3

    auto downed = f.fighter(1);
    downed.current_hp = 0;

    // Init: Aria 1 (skipped, down), Tam 20, Goblin 1. Cure Wounds 1d8 = 5 (+3).
    ScriptedRandomSource rolls({1, 20, 1, 5});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(downed, {}, {});
    encounter.add_character(cleric, {cure}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current()->name == "Brother Tam");

    REQUIRE(encounter.spell_target_candidates(cure) == std::vector<int>{0, 1});
    REQUIRE(encounter.cast_spell(cure.spell_id, 1, {0}));
    auto events = encounter.take_events();
    REQUIRE(logs(events).at(1) == "    Aria: regains 8 HP. Now 8/12 HP.");
    REQUIRE(encounter.character(1)->current_hp == 8);
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
}
