#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/encounter.hpp"
#include "dnd/repository.hpp"
#include "dnd/spellcasting.hpp"

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

    // Aria's equipment: chain mail (AC 16) and a longsword, both equipped.
    std::vector<dnd::InventoryEntry> fighter_gear(std::vector<dnd::InventoryEntry> extra = {}) {
        std::vector<dnd::InventoryEntry> gear = {{item("Chain Mail"), 1, true}, {item("Longsword"), 1, true}};
        gear.insert(gear.end(), extra.begin(), extra.end());
        return gear;
    }

    // A level-1 fighter; with fighter_gear(), AC 16 and a longsword: +5 to hit, 1d10+3 two-handed.
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
        return c;
    }

    // A level-1 wizard: INT 16, spell attack +5, save DC 13, two 1st-level slots; AC 12 unarmored.
    dnd::Character wizard(int id = 2) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = "Zanna";
        c.class_id = class_id("Wizard");
        c.intelligence = 16;
        c.dexterity = 14;
        c.max_hp = 8;
        c.current_hp = 8;
        return c;
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
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 2);

    const auto& combatants = encounter.combatants();
    REQUIRE(combatants.size() == 3);
    REQUIRE(combatants[0].name == "Aria");
    REQUIRE(combatants[0].attack_bonus == 5);  // +2 proficiency, +3 STR
    REQUIRE(combatants[0].attack_name == "Longsword");
    REQUIRE(combatants[0].armor_class == 16);                // chain mail
    REQUIRE(combatants[0].damage_dice_sides == 10);          // versatile, held two-handed (no shield)
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
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
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
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
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
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.dodge());
    REQUIRE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::Dodging));
    REQUIRE(encounter.status_text(0) == "Up (Dodging)");
    encounter.next_turn();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Goblin attacks Aria: d20(2) +4 with disadvantage (target dodging) -> miss."));
    REQUIRE_FALSE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::Dodging));  // cleared at the start of Aria's turn
}

TEST_CASE("attacking to victory awards XP and loot", "[encounter]") {
    Fixture f;
    // Init: Aria 20, Goblin 1. Aria's attack: d20 15 (+5 = 20 hit), 1d8 = 8 (+3 = 11 >= 7 HP).
    ScriptedRandomSource rolls({20, 1, 15, 8});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
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

TEST_CASE("victory can make a level available", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1, 15, 8});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.experience = 280;  // 300 is level 2
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.attack(1, 5, 1, 8, 3);

    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "*** Aria has enough XP for level 2! Use Level Up on the main window. ***"));
    REQUIRE(encounter.character(1)->level == 1);  // the player levels up (dnd/level_up.hpp)
    REQUIRE(encounter.character(1)->experience == 330);
}

TEST_CASE("victory coins are split among the survivors", "[encounter][currency]") {
    Fixture f;
    // Init: Aria 20, Zanna 1, Goblin 1 (Aria, then Goblin before Zanna on the tie? Aria first either way).
    ScriptedRandomSource rolls({20, 1, 1, 15, 8});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(1), {}, f.fighter_gear());
    encounter.add_character(f.wizard(2), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.set_victory_reward("", {}, 1001);  // 10 gp, 1 cp
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current()->name == "Aria");
    REQUIRE(encounter.attack(2, 5, 1, 8, 3));
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "The party finds 10 gp, 1 cp: Aria 5 gp, 1 cp; Zanna 5 gp."));
    REQUIRE(encounter.character(1)->coins == std::array<int, 5>{1, 0, 0, 5, 0});
    REQUIRE(encounter.character(2)->coins == std::array<int, 5>{0, 0, 0, 5, 0});
}

TEST_CASE("defeat when every character is down", "[encounter]") {
    Fixture f;
    // Init: Aria 1, Orc 20. Orc: pick, d20 20 (crit), 2d12 = 12 + 12 + 3 = 27.
    ScriptedRandomSource rolls({1, 20, 1, 20, 12, 12});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();

    auto events = encounter.take_events();
    auto lines = logs(events);
    REQUIRE(contains(lines, "Orc attacks Aria: d20(20) +5 -> CRITICAL HIT for 27. Aria is now 0/12 HP."));
    REQUIRE(lines.back() == "--- Defeat! All characters have fallen. ---");
    REQUIRE(contains(lines, "*** Aria is killed outright by massive damage. ***"));  // 15 left over >= 12 max HP
    REQUIRE(encounter.status() == dnd::EncounterStatus::Defeat);
    REQUIRE(encounter.character(1)->current_hp == 0);
    REQUIRE(encounter.character(1)->dead);
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
}

TEST_CASE("next_turn skips combatants who are down", "[encounter]") {
    Fixture f;
    // Init: Aria 20, Goblin #1 10, Goblin #2 5. Aria kills #1 (d20 15, 1d8 8).
    // Next turn: #1 is skipped; #2 acts (pick, d20 1 miss); back to Aria.
    ScriptedRandomSource rolls({20, 10, 5, 15, 8, 1, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
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
    encounter.add_character(aria, {}, f.fighter_gear({{potion, 1}, {f.item("Torch"), 5}}));
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
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
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

TEST_CASE("joining at 0 HP means unconscious but stable", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({10});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 0;
    encounter.add_character(aria, {}, f.fighter_gear());
    REQUIRE(encounter.status_text(0) == "Stable");
    REQUIRE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Unconscious));
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
    encounter.add_character(downed, {}, f.fighter_gear());
    encounter.add_character(cleric, {cure}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current()->name == "Brother Tam");

    REQUIRE(encounter.spell_target_candidates(cure) == std::vector<int>{0, 1});
    REQUIRE(encounter.cast_spell(cure.spell_id, 1, {0}));
    auto events = encounter.take_events();
    REQUIRE(logs(events).at(1) == "    Aria: regains 8 HP. Now 8/12 HP.");
    REQUIRE(logs(events).at(2) == "    Aria regains consciousness.");
    REQUIRE(encounter.status_text(0) == "Up");
    REQUIRE(encounter.character(1)->current_hp == 8);
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
}

// --- Effects system phase 2 --------------------------------------------------

namespace {
// Aria (12 HP, AC 16) and Zanna (8 HP, AC 12) against an Orc (+5, 1d12+3).
// Initiative: Aria 1+1, Zanna 2+2, Orc 20+1 -> Orc, Zanna, Aria. The Orc picks
// Aria (1 of 2) and hits for 1d12=5 +3 = 8, dropping her from 3 HP to 0.
std::vector<int> orc_drops_aria() { return {1, 2, 20, 1, 15, 5}; }
}  // namespace

TEST_CASE("dropping to 0 HP: dying, and monsters ignore the dying", "[encounter][effects]") {
    Fixture f;
    auto script = orc_drops_aria();
    script.insert(script.end(), {9, 1, 2, 1, 1, 2});  // Aria's saves: 9, then a natural 1; Orc misses Zanna
    ScriptedRandomSource rolls(script);
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 3;
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_character(f.wizard(), {}, {});
    encounter.add_monsters(f.monster("Orc"), 1);

    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Orc attacks Aria: d20(15) +5 -> hit for 8. Aria is now 0/12 HP."));
    REQUIRE(contains(lines, "    Aria falls unconscious and is dying."));
    REQUIRE(encounter.status_text(0) == "Dying ○○○ ○○○");
    REQUIRE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Unconscious));
    REQUIRE(encounter.current()->name == "Zanna");

    REQUIRE(encounter.dash());
    encounter.next_turn();  // Aria rolls a death save, then the Orc goes for Zanna, not the dying Aria
    lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Aria makes a death saving throw: d20(9) -> failure (0 successes, 1 failures)."));
    REQUIRE(contains(lines, "Orc attacks Zanna"));
    REQUIRE(encounter.status_text(0) == "Dying ○○○ ✗○○");

    REQUIRE(encounter.dash());
    encounter.next_turn();  // natural 1: two more failures, dead
    auto events = encounter.take_events();
    lines = logs(events);
    REQUIRE(contains(lines, "Aria makes a death saving throw: d20(1) -> natural 1. Aria has failed three death saves."));
    REQUIRE(contains(lines, "*** Aria dies. ***"));
    REQUIRE(encounter.character(1)->dead);
    REQUIRE(encounter.status_text(0) == "Dead");
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
    REQUIRE(encounter.status() == dnd::EncounterStatus::InProgress);  // Zanna's still up
}

TEST_CASE("a natural 20 death save wakes a character up for their turn", "[encounter][effects]") {
    Fixture f;
    auto script = orc_drops_aria();
    script.push_back(20);
    ScriptedRandomSource rolls(script);
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 3;
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_character(f.wizard(), {}, {});
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();
    encounter.dash();  // Zanna
    encounter.take_events();

    encounter.next_turn();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Aria makes a death saving throw: d20(20) -> natural 20! Aria regains 1 HP and wakes up."));
    REQUIRE(encounter.current_index() == 0);
    REQUIRE(encounter.awaiting_action());
    REQUIRE(encounter.combatants()[0].current_hp == 1);
    REQUIRE_FALSE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Unconscious));
}

TEST_CASE("defeat stabilizes anyone still dying", "[encounter][effects]") {
    Fixture f;
    ScriptedRandomSource rolls({1, 20, 1, 15, 5});  // Aria 2, Orc 21; Orc hits Aria (3 HP) for 8
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 3;
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();

    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "--- Defeat! All characters have fallen. ---"));
    REQUIRE(lines.back() == "Aria is stabilized.");
    REQUIRE(encounter.status_text(0) == "Stable");
    REQUIRE_FALSE(encounter.character(1)->dead);
    REQUIRE(encounter.effects().empty());  // combat effects end with the fight
}

TEST_CASE("dead characters don't act, can't be healed, and earn no XP", "[encounter][effects]") {
    Fixture f;
    auto fire_bolt = f.spell("Fire Bolt");
    auto cure = f.spell("Cure Wounds");
    ScriptedRandomSource rolls({5, 20, 1, 18, 10});  // Zanna first; Fire Bolt d20 18, 1d10 = 10
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.dead = true;
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_character(f.wizard(), {fire_bolt, cure}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.status_text(0) == "Dead");
    REQUIRE(encounter.current()->name == "Zanna");
    REQUIRE(encounter.spell_target_candidates(cure) == std::vector<int>{1});  // not the dead Aria

    REQUIRE(encounter.cast_spell(fire_bolt.spell_id, 0, {2}));
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "The party earns 50 XP (50 each)."));
    REQUIRE(encounter.character(1)->experience == 0);
    REQUIRE(encounter.character(2)->experience == 50);
}

TEST_CASE("prone, bonus dice, and effect durations", "[encounter][effects]") {
    Fixture f;
    // Init Aria 20, Goblin 1. Aria: Bless d4 = 3, advantage d20s 5 and 12, d8 = 1.
    // Goblin: stands up, picks Aria, d20 2 (miss).
    ScriptedRandomSource rolls({20, 1, 3, 5, 12, 1, 1, 2});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    dnd::ActiveEffect prone;
    prone.kind = dnd::EffectKind::Condition;
    prone.condition = dnd::Condition::Prone;
    prone.target = 1;
    encounter.add_effect(prone);

    dnd::ActiveEffect bless;
    bless.kind = dnd::EffectKind::AttackBonusDice;
    bless.amount = 4;
    bless.target = 0;
    bless.source = 0;
    bless.duration.kind = dnd::DurationKind::Rounds;
    bless.duration.rounds = 1;
    bless.label = "Blessed";
    encounter.add_effect(bless);
    REQUIRE(encounter.status_text(1) == "Up (Prone)");

    REQUIRE(encounter.attack(1, 5, 1, 8, 3));
    auto lines = logs(encounter.take_events());
    REQUIRE(lines.at(0) ==
            "Aria attacks Goblin: d20(12) +5 +1d4(3) with advantage (target prone) -> hit for 4. Goblin is now 3/7 HP.");

    encounter.next_turn();
    lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Goblin stands up."));
    REQUIRE(contains(lines, "Blessed on Aria ends."));  // 1 round, counted on Aria's turn
    REQUIRE(encounter.current_index() == 0);
    REQUIRE(encounter.effects().empty());

    SECTION("an incapacitated creature's turn is skipped") {
        dnd::ActiveEffect stunned;
        stunned.kind = dnd::EffectKind::Condition;
        stunned.condition = dnd::Condition::Stunned;
        stunned.target = 1;
        encounter.add_effect(stunned);
        encounter.dash();
        encounter.next_turn();
        REQUIRE(logs(encounter.take_events()) ==
                std::vector<std::string>{"Aria takes the Dash action.", "Goblin is stunned and can't act."});
        REQUIRE(encounter.current_index() == 0);
    }

    SECTION("armor class bonuses") {
        dnd::ActiveEffect shield;
        shield.kind = dnd::EffectKind::ArmorClassBonus;
        shield.amount = 2;
        shield.target = 0;
        encounter.add_effect(shield);
        REQUIRE(encounter.armor_class(0) == 18);
    }
}

TEST_CASE("taking damage tests concentration", "[encounter][effects]") {
    Fixture f;
    // Zanna 1+2, Orc 20+1. Orc: pick, d20 15 hits AC 12, 1d12 = 1 (+3 = 4). CON save d20 3 vs DC 10: fails.
    ScriptedRandomSource rolls({1, 20, 1, 15, 1, 3});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.wizard(), {}, {});
    encounter.add_monsters(f.monster("Orc"), 1);

    encounter.start_concentration(0, "Bless");
    dnd::ActiveEffect bless;
    bless.kind = dnd::EffectKind::AttackBonusDice;
    bless.amount = 4;
    bless.target = 0;
    bless.source = 0;
    bless.concentration = true;
    bless.label = "Blessed";
    encounter.add_effect(bless);
    REQUIRE(encounter.status_text(0) == "Up (Blessed, Concentrating: Bless)");

    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "    Zanna makes a DC 10 concentration save: CON save d20(3) +0 -> fails."));
    REQUIRE(contains(lines, "    Zanna stops concentrating on Bless (failed the save)."));
    REQUIRE(encounter.effects().empty());
    REQUIRE(encounter.status_text(0) == "Up");
}

TEST_CASE("a charmed monster won't attack its charmer; harming it breaks the charm", "[encounter][effects]") {
    Fixture f;
    ScriptedRandomSource rolls({1, 20, 15, 1});  // Goblin first; Aria then hits for 1d8 = 1 (+3 = 4)
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 1);

    dnd::ActiveEffect charmed;
    charmed.kind = dnd::EffectKind::Condition;
    charmed.condition = dnd::Condition::Charmed;
    charmed.target = 1;
    charmed.source = 0;
    encounter.add_effect(charmed);

    encounter.roll_initiative();
    REQUIRE(contains(logs(encounter.take_events()), "Goblin is charmed and won't attack."));

    REQUIRE(encounter.attack(1, 5, 1, 8, 3));
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "    Goblin is no longer charmed."));
    REQUIRE(encounter.effects().empty());
}

// --- Effects system phase 3: spells and monsters apply effects ---------------

namespace {
// Aria (idx 0), Zanna (idx 1), Goblin (idx 2). Initiative Aria 10+1, Zanna 20+2,
// Goblin 1+2: Zanna acts first, then Aria, then the Goblin.
struct PartyVsGoblin {
    Fixture f;
    ScriptedRandomSource rolls;
    dnd::Dice dice;
    dnd::Encounter encounter;

    PartyVsGoblin(std::vector<int> after_initiative, std::vector<std::string> zanna_spells,
                  std::vector<dnd::InventoryEntry> zanna_inventory = {})
        : rolls(with_initiative(std::move(after_initiative))), dice(rolls), encounter(f.encounter(dice)) {
        std::vector<dnd::Spell> spells;
        for (const auto& name : zanna_spells) spells.push_back(f.spell(name));
        encounter.add_character(f.fighter(), {}, f.fighter_gear());
        encounter.add_character(f.wizard(), spells, std::move(zanna_inventory));
        encounter.add_monsters(f.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
    }

    static std::vector<int> with_initiative(std::vector<int> rest) {
        std::vector<int> all = {10, 20, 1};
        all.insert(all.end(), rest.begin(), rest.end());
        return all;
    }

    int id(const std::string& spell) { return f.spell(spell).spell_id; }
};
}  // namespace

TEST_CASE("Guiding Bolt: the next attack against the target has advantage", "[encounter][effects][spells]") {
    SECTION("used by the next attack") {
        // Guiding Bolt d20 15 hits AC 15; 4d6 = 4. Aria: advantage d20s 2 and 17, d8 = 1.
        PartyVsGoblin fight({15, 1, 1, 1, 1, 2, 17, 1}, {"Guiding Bolt"});
        REQUIRE(fight.encounter.cast_spell(fight.id("Guiding Bolt"), 1, {2}));
        auto lines = logs(fight.encounter.take_events());
        REQUIRE(contains(lines, "    Goblin: d20(15) +5 -> hit for 4; Guiding Bolt. Now 3/7 HP."));

        fight.encounter.next_turn();  // Zanna's turn ends, but the effect lasts until the end of her *next* turn
        REQUIRE(dnd::has_effect(fight.encounter.effects(), 2, dnd::EffectKind::AdvantageOnNextAttackAgainst));
        REQUIRE(fight.encounter.attack(2, 5, 1, 8, 3));
        lines = logs(fight.encounter.take_events());
        REQUIRE(contains(lines, "Aria attacks Goblin: d20(17) +5 with advantage (guiding bolt) -> hit for 4."));
    }

    SECTION("expires at the end of the caster's next turn") {
        // Guiding Bolt hits for 4; Goblin picks Aria and rolls a natural 1.
        PartyVsGoblin fight({15, 1, 1, 1, 1, 1, 1}, {"Guiding Bolt"});
        fight.encounter.cast_spell(fight.id("Guiding Bolt"), 1, {2});
        fight.encounter.next_turn();  // -> Aria
        fight.encounter.dash();
        fight.encounter.next_turn();  // -> Goblin misses, -> Zanna
        REQUIRE(fight.encounter.current()->name == "Zanna");
        REQUIRE(dnd::has_effect(fight.encounter.effects(), 2, dnd::EffectKind::AdvantageOnNextAttackAgainst));
        fight.encounter.dash();
        fight.encounter.take_events();
        fight.encounter.next_turn();  // end of Zanna's next turn
        REQUIRE(contains(logs(fight.encounter.take_events()), "Guiding Bolt on Goblin ends."));
        REQUIRE(fight.encounter.effects().empty());
    }
}

TEST_CASE("Bless: bonus dice for allies, lost with concentration", "[encounter][effects][spells]") {
    // Bless (no roll). Aria: Bless d4 3, d20 10, d8 1. Goblin: picks Zanna (2 of 2), d20 15 hits
    // AC 12, 1d6 = 1 (+2 = 3). Zanna's DC 10 CON save: d20 2, Bless d4 1 -> 3, fails.
    PartyVsGoblin fight({3, 10, 1, 2, 15, 1, 2, 1}, {"Bless"});
    auto& encounter = fight.encounter;

    REQUIRE(encounter.cast_spell(fight.id("Bless"), 1, {0, 1}));
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "    Aria: Blessed."));
    REQUIRE(contains(lines, "    Zanna: Blessed."));
    REQUIRE(encounter.status_text(0) == "Up (Blessed, Blessed)");  // attack and save dice
    REQUIRE(encounter.combatants()[1].concentration == "Bless");

    encounter.next_turn();
    REQUIRE(encounter.attack(2, 5, 1, 8, 3));
    REQUIRE(contains(logs(encounter.take_events()), "Aria attacks Goblin: d20(10) +5 +1d4(3) -> hit for 4. Goblin is now 3/7 HP."));

    encounter.next_turn();  // the Goblin hits Zanna, who loses concentration
    lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "    Zanna makes a DC 10 concentration save: CON save d20(2) +0 +1d4(1) -> fails."));
    REQUIRE(contains(lines, "    Zanna stops concentrating on Bless (failed the save)."));
    REQUIRE(encounter.effects().empty());
    REQUIRE(encounter.status_text(0) == "Up");

    SECTION("Bless takes one more creature per slot level above 1st") {
        auto bless = fight.f.spell("Bless");
        REQUIRE(dnd::spell_max_targets(bless, 1, 1) == 3);
        REQUIRE(dnd::spell_max_targets(bless, 1, 2) == 4);
    }
}

TEST_CASE("Vicious Mockery: disadvantage on the target's next attack", "[encounter][effects][spells]") {
    // 1d4 = 1; Goblin WIS save d20 5 (-1) fails. Later the Goblin picks Aria; disadvantage d20s 18 and 3.
    PartyVsGoblin fight({1, 5, 1, 18, 3}, {"Vicious Mockery"});
    auto& encounter = fight.encounter;
    REQUIRE(encounter.cast_spell(fight.id("Vicious Mockery"), 0, {2}));
    REQUIRE(contains(logs(encounter.take_events()),
                     "    Goblin: WIS save d20(5) -1 vs DC 13 -> failed, takes 1; Vicious Mockery. Now 6/7 HP."));
    encounter.next_turn();
    encounter.dash();
    encounter.next_turn();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Goblin attacks Aria: d20(3) +4 with disadvantage (vicious mockery) -> miss."));
    REQUIRE_FALSE(dnd::has_effect(encounter.effects(), 2, dnd::EffectKind::DisadvantageOnNextAttack));
}

TEST_CASE("Charm Person, Faerie Fire, Shield of Faith, Divine Favor", "[encounter][effects][spells]") {
    SECTION("Charm Person: save with advantage; charmed monsters won't attack the charmer") {
        PartyVsGoblin fight({3, 4}, {"Charm Person"});  // advantage d20s 3 and 4 -> 4 - 1 = 3, fails
        REQUIRE(fight.encounter.cast_spell(fight.id("Charm Person"), 1, {2}));
        auto lines = logs(fight.encounter.take_events());
        REQUIRE(contains(lines, "    Goblin: WIS save d20(4) -1 with advantage (in combat with the caster) vs DC 13 "
                                "-> failed; Charmed. Now 7/7 HP."));
        REQUIRE(dnd::has_condition(fight.encounter.effects(), 2, dnd::Condition::Charmed));
    }

    SECTION("Faerie Fire: no damage; attacks against a failed target have advantage") {
        PartyVsGoblin fight({1}, {"Faerie Fire"});  // DEX save d20 1 fails
        REQUIRE(fight.encounter.cast_spell(fight.id("Faerie Fire"), 1, {2}));
        auto lines = logs(fight.encounter.take_events());
        REQUIRE(contains(lines, "    Goblin: DEX save d20(1) +2 vs DC 13 -> failed; Faerie Fire. Now 7/7 HP."));
        REQUIRE_FALSE(contains(lines, "Damage roll"));
        std::vector<bool> alive(3, true);
        auto mods = dnd::attack_roll_modifiers(fight.encounter.effects(), 0, 2, true, alive);
        REQUIRE(mods.mode() == dnd::RollMode::Advantage);
        REQUIRE(fight.encounter.combatants()[1].concentration == "Faerie Fire");
    }

    SECTION("Shield of Faith: +2 AC") {
        PartyVsGoblin fight({}, {"Shield of Faith"});
        REQUIRE(fight.encounter.cast_spell(fight.id("Shield of Faith"), 1, {0}));
        REQUIRE(fight.encounter.armor_class(0) == 18);
    }

    SECTION("Divine Favor: self only, no target needed") {
        PartyVsGoblin fight({}, {"Divine Favor"});
        auto favor = fight.f.spell("Divine Favor");
        REQUIRE(fight.encounter.spell_target_candidates(favor).empty());
        REQUIRE(fight.encounter.cast_spell(favor.spell_id, 1, {}));
        REQUIRE(contains(logs(fight.encounter.take_events()), "    Zanna: Divine Favor."));
        REQUIRE(dnd::has_effect(fight.encounter.effects(), 1, dnd::EffectKind::ExtraWeaponDamageDice));
        REQUIRE(fight.encounter.status_text(1) == "Up (Divine Favor, Concentrating: Divine Favor)");
    }

    SECTION("a new concentration spell ends the old one") {
        PartyVsGoblin fight({}, {"Shield of Faith", "Divine Favor"});
        fight.encounter.cast_spell(fight.id("Shield of Faith"), 1, {0});
        fight.encounter.take_events();
        // Next round, Zanna switches to Divine Favor.
        fight.encounter.next_turn();
        fight.encounter.dash();
        fight.encounter.next_turn();  // Goblin (rolls repeat the last scripted value: 1s)
        fight.encounter.take_events();
        REQUIRE(fight.encounter.current()->name == "Zanna");
        REQUIRE(fight.encounter.cast_spell(fight.id("Divine Favor"), 1, {}));
        auto lines = logs(fight.encounter.take_events());
        REQUIRE(contains(lines, "    Zanna stops concentrating on Shield of Faith (starts concentrating on Divine Favor)."));
        REQUIRE(fight.encounter.armor_class(0) == 16);  // Shield of Faith is gone
        REQUIRE(fight.encounter.combatants()[1].concentration == "Divine Favor");
    }
}

TEST_CASE("a Wolf's bite can knock its target prone", "[encounter][effects]") {
    Fixture f;
    // Aria 1+1, Wolf 20+2. Wolf: pick, d20 15 hits AC 16, 2d4 = 1 + 1 (+2); STR save d20 2 (+3) vs 11 fails.
    ScriptedRandomSource rolls({1, 20, 1, 15, 1, 1, 2});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Wolf"), 1);
    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Wolf attacks Aria: d20(15) +4 -> hit for 4. Aria is now 8/12 HP."));
    REQUIRE(contains(lines, "    Aria: STR save d20(2) +3 vs DC 11 -> failed, prone."));
    REQUIRE(contains(lines, "Aria stands up."));  // at the start of her turn
    REQUIRE(encounter.effects().empty());
}

TEST_CASE("stabilizing a dying ally", "[encounter][effects]") {
    Fixture f;
    auto setup = [&f](dnd::Dice& dice, std::vector<dnd::Spell> spells, std::vector<dnd::InventoryEntry> inventory) {
        auto encounter = f.encounter(dice);
        auto aria = f.fighter();
        aria.current_hp = 3;
        encounter.add_character(aria, {}, f.fighter_gear());
        encounter.add_character(f.wizard(), std::move(spells), std::move(inventory));
        encounter.add_monsters(f.monster("Orc"), 1);
        encounter.roll_initiative();  // the Orc drops Aria; Zanna's turn
        encounter.take_events();
        return encounter;
    };

    SECTION("a successful Medicine check") {
        auto script = orc_drops_aria();
        script.push_back(12);
        ScriptedRandomSource rolls(script);
        dnd::Dice dice(rolls);
        auto encounter = setup(dice, {}, {});
        REQUIRE(encounter.stabilize_targets() == std::vector<int>{0});
        REQUIRE(encounter.stabilize(0));
        REQUIRE(contains(logs(encounter.take_events()),
                         "Zanna tries to stabilize Aria: WIS (Medicine) check d20(12) +0 vs DC 10 -> success, Aria is stable."));
        REQUIRE(encounter.status_text(0) == "Stable");
        REQUIRE(encounter.stabilize_targets().empty());
    }

    SECTION("a failed check leaves them dying") {
        auto script = orc_drops_aria();
        script.push_back(5);
        ScriptedRandomSource rolls(script);
        dnd::Dice dice(rolls);
        auto encounter = setup(dice, {}, {});
        REQUIRE(encounter.stabilize(0));
        REQUIRE(contains(logs(encounter.take_events()), "-> failure."));
        REQUIRE(encounter.status_text(0) == "Dying ○○○ ○○○");
    }

    SECTION("a Healer's Kit makes it automatic") {
        ScriptedRandomSource rolls(orc_drops_aria());
        dnd::Dice dice(rolls);
        auto encounter = setup(dice, {}, {{f.item("Healer's Kit"), 1}});
        REQUIRE(encounter.has_healers_kit());
        REQUIRE(encounter.stabilize(0));
        REQUIRE(contains(logs(encounter.take_events()), "Zanna uses a Healer's Kit on Aria: Aria is stable."));
        REQUIRE(encounter.status_text(0) == "Stable");
    }

    SECTION("Spare the Dying") {
        ScriptedRandomSource rolls(orc_drops_aria());
        dnd::Dice dice(rolls);
        auto spare = f.spell("Spare the Dying");
        auto encounter = setup(dice, {spare}, {});
        REQUIRE(encounter.spell_target_candidates(spare) == std::vector<int>{0});
        REQUIRE(encounter.cast_spell(spare.spell_id, 0, {0}));
        REQUIRE(contains(logs(encounter.take_events()), "    Aria: stable."));
        REQUIRE(encounter.status_text(0) == "Stable");
    }
}

TEST_CASE("effects that block healing", "[encounter][effects]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1, 3, 3});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 2;
    auto potion = f.item("Potion of Healing");
    encounter.add_character(aria, {}, f.fighter_gear({{potion, 1}}));
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    dnd::ActiveEffect chill;
    chill.kind = dnd::EffectKind::CantRegainHitPoints;
    chill.target = 0;
    chill.label = "Chill Touch";
    encounter.add_effect(chill);

    REQUIRE(encounter.drink_potion(potion.item_id));
    auto lines = logs(encounter.take_events());
    REQUIRE(lines.at(0) == "Aria drinks a Potion of Healing and regains 0 HP (now 2/12).");
    REQUIRE(lines.at(1) == "    Aria can't regain hit points right now.");
}

TEST_CASE("status tooltips explain effects in plain language", "[encounter][effects]") {
    Fixture f;
    ScriptedRandomSource rolls({10, 20, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(), {}, f.fighter_gear());
    encounter.add_character(f.wizard(), {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();  // Zanna's turn

    REQUIRE(encounter.status_tooltip(0) == std::vector<std::string>{"No conditions or effects."});

    encounter.start_concentration(1, "Bless");
    dnd::ActiveEffect bless;
    bless.kind = dnd::EffectKind::AttackBonusDice;
    bless.amount = 4;
    bless.target = 0;
    bless.source = 1;
    bless.concentration = true;
    bless.duration.kind = dnd::DurationKind::Rounds;
    bless.duration.rounds = 10;
    bless.label = "Blessed";
    encounter.add_effect(bless);

    dnd::ActiveEffect prone;
    prone.kind = dnd::EffectKind::Condition;
    prone.condition = dnd::Condition::Prone;
    prone.target = 2;
    prone.source = 0;
    encounter.add_effect(prone);

    REQUIRE(encounter.status_tooltip(0) ==
            std::vector<std::string>{"Blessed: Adds 1d4 to its attack rolls. (from Zanna; 10 rounds left; while Zanna concentrates)"});
    auto caster = encounter.status_tooltip(1);
    REQUIRE(caster.size() == 1);
    REQUIRE(caster[0].rfind("Concentrating on Bless: taking damage forces a CON save", 0) == 0);
    auto goblin = encounter.status_tooltip(2);
    REQUIRE(goblin.size() == 2);
    REQUIRE(goblin[0].rfind("Prone: Its attacks have disadvantage.", 0) == 0);
    REQUIRE(goblin[1] == "Humanoid.");  // its creature type and traits
    REQUIRE(goblin[0].find("(from Aria)") != std::string::npos);
}

TEST_CASE("status tooltips for dying, stable, and dead", "[encounter][effects]") {
    Fixture f;
    ScriptedRandomSource rolls({10});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto stable = f.fighter(1);
    stable.current_hp = 0;
    auto dead = f.wizard(2);
    dead.dead = true;
    encounter.add_character(stable, {}, f.fighter_gear());
    encounter.add_character(dead, {}, {});

    auto lines = encounter.status_tooltip(0);
    REQUIRE(lines.at(0) == "Stable: unconscious at 0 HP, but no longer dying. Any healing wakes them.");
    REQUIRE(lines.at(1).rfind("Unconscious: Incapacitated and prone.", 0) == 0);
    REQUIRE(encounter.status_tooltip(1) == std::vector<std::string>{"Dead. Only revival magic can bring them back."});
}

// --- Equipment ---------------------------------------------------------------

TEST_CASE("combatants' AC and attacks come from their equipment", "[encounter][equipment]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 10, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(1), {}, {{f.item("Chain Mail"), 1, true}, {f.item("Shield"), 1, true},
                                              {f.item("Longsword"), 1, true}});
    encounter.add_character(f.wizard(2), {}, {});  // nothing equipped
    encounter.add_monsters(f.monster("Goblin"), 1);

    const auto& aria = encounter.combatants()[0];
    REQUIRE(aria.armor_class == 18);  // chain mail + shield
    REQUIRE(aria.attack_name == "Longsword");
    REQUIRE(aria.damage_dice_sides == 8);  // one-handed with a shield

    const auto& zanna = encounter.combatants()[1];
    REQUIRE(zanna.armor_class == 12);  // 10 + DEX
    REQUIRE(zanna.attack_name == "Unarmed Strike");
    REQUIRE(zanna.damage_dice_count == 0);
    REQUIRE(zanna.damage_bonus == 1);  // 1 + STR +0
}

TEST_CASE("armor without proficiency: disadvantage and no spellcasting", "[encounter][equipment]") {
    Fixture f;
    auto fire_bolt = f.spell("Fire Bolt");
    ScriptedRandomSource rolls({20, 1});  // Zanna first
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.wizard(), {fire_bolt}, {{f.item("Plate Armor"), 1, true}});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.combatants()[0].armor_not_proficient);
    REQUIRE(encounter.combatants()[0].armor_class == 18);
    REQUIRE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::ArmorNotProficient));
    REQUIRE(encounter.status_text(0) == "Up (Armor not proficient)");
    REQUIRE(encounter.castable_spells().empty());
    REQUIRE_FALSE(encounter.cast_spell(fire_bolt.spell_id, 0, {1}));
    REQUIRE(logs(encounter.take_events()) ==
            std::vector<std::string>{"Zanna can't cast spells while wearing armor they aren't proficient with."});
    REQUIRE(encounter.awaiting_action());  // the action isn't used up
}

// --- Turn economy: actions, bonus actions, Extra Attack ------------------------

TEST_CASE("Extra Attack: two attacks per Attack action from level 5", "[encounter][turn]") {
    Fixture f;
    // Aria first (20), Goblin #1 and #2 slow. Both attacks: d20 2 (miss), then misses keep rolling 2.
    ScriptedRandomSource rolls({20, 1, 1, 2});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.level = 5;
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 2);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.combatants()[0].attacks_per_action == 2);

    SECTION("both attacks, the second at a different target") {
        REQUIRE(encounter.attack(1, 5, 1, 8, 3));
        REQUIRE(contains(logs(encounter.take_events()), "    (Extra Attack: 1 more attack in this Attack action.)"));
        REQUIRE(encounter.turn().actions_left == 0);
        REQUIRE(encounter.turn().attacks_left == 1);
        REQUIRE(encounter.awaiting_action());
        REQUIRE(encounter.attack(2, 5, 1, 8, 3));
        REQUIRE_FALSE(encounter.awaiting_action());
        encounter.take_events();
        REQUIRE_FALSE(encounter.attack(1, 5, 1, 8, 3));
        REQUIRE(logs(encounter.take_events()) == std::vector<std::string>{"Aria has no action left this turn."});
    }

    SECTION("another action gives up the remaining attack") {
        REQUIRE(encounter.attack(1, 5, 1, 8, 3));
        encounter.take_events();
        REQUIRE_FALSE(encounter.dash());  // the action is already spent on the Attack action
        REQUIRE(logs(encounter.take_events()) ==
                std::vector<std::string>{"Aria has no action left this turn (only the rest of the Attack action)."});
        REQUIRE(encounter.turn().attacks_left == 1);
    }
}

TEST_CASE("bonus-action spells and the one-levelled-spell rule", "[encounter][turn]") {
    Fixture f;
    auto healing_word = f.spell("Healing Word");
    auto sacred_flame = f.spell("Sacred Flame");
    auto guiding_bolt = f.spell("Guiding Bolt");
    auto feather_fall = f.spell("Feather Fall");

    auto cleric = f.wizard(3);
    cleric.character_name = "Tam";
    cleric.class_id = f.class_id("Cleric");
    cleric.wisdom = 16;
    auto aria = f.fighter(1);
    aria.current_hp = 5;

    // Aria 1+1, Tam 20+2, Goblin 1+2: Tam first. Every later roll is 1.
    ScriptedRandomSource rolls({1, 20, 1, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_character(cleric, {healing_word, sacred_flame, guiding_bolt, feather_fall}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current()->name == "Tam");
    REQUIRE(encounter.bonus_action_available());

    auto names = [&]() {
        std::vector<std::string> result;
        for (const auto& spell : encounter.castable_spells()) result.push_back(spell.name);
        return result;
    };
    REQUIRE(names() == std::vector<std::string>{"Healing Word", "Sacred Flame", "Guiding Bolt"});  // no Feather Fall

    SECTION("Healing Word, then only an action cantrip") {
        REQUIRE(encounter.cast_spell(healing_word.spell_id, 1, {0}));
        REQUIRE_FALSE(encounter.bonus_action_available());
        REQUIRE(encounter.awaiting_action());  // the action is still there
        REQUIRE(names() == std::vector<std::string>{"Sacred Flame"});
        encounter.take_events();
        REQUIRE_FALSE(encounter.cast_spell(guiding_bolt.spell_id, 1, {2}));
        REQUIRE(logs(encounter.take_events()) ==
                std::vector<std::string>{"After casting a bonus-action spell, only a cantrip can be cast this turn."});
        REQUIRE(encounter.cast_spell(sacred_flame.spell_id, 0, {2}));
        REQUIRE_FALSE(encounter.awaiting_action());
    }

    SECTION("Guiding Bolt first rules out Healing Word") {
        REQUIRE(encounter.cast_spell(guiding_bolt.spell_id, 1, {2}));
        REQUIRE(encounter.bonus_action_available());
        REQUIRE(names().empty());
        encounter.take_events();
        REQUIRE_FALSE(encounter.cast_spell(healing_word.spell_id, 1, {0}));
        REQUIRE(contains(logs(encounter.take_events()), "A bonus-action spell can only be paired with a cantrip"));
    }
}

// --- Class features ------------------------------------------------------------

namespace {
using BonusAction = dnd::Encounter::BonusAction;

// A level-`level` character of `class_name` with Aria's stats (STR 16, DEX 12, CON 14).
dnd::Character classed(Fixture& f, int id, const char* name, const char* class_name, int level = 1) {
    auto c = f.fighter(id);
    c.character_name = name;
    c.class_id = f.class_id(class_name);
    c.level = level;
    return c;
}

bool offers(const dnd::Encounter& encounter, BonusAction kind) {
    auto options = encounter.bonus_action_options();
    return std::any_of(options.begin(), options.end(), [kind](const auto& o) { return o.kind == kind; });
}
}  // namespace

TEST_CASE("Second Wind and Action Surge", "[encounter][class_features]") {
    Fixture f;
    ScriptedRandomSource rolls({20, 1, 6});  // Aria first; Second Wind d10 = 6
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = classed(f, 1, "Aria", "Fighter", 2);
    aria.current_hp = 3;
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(offers(encounter, BonusAction::SecondWind));
    REQUIRE(encounter.use_bonus_action(BonusAction::SecondWind));
    auto events = encounter.take_events();
    REQUIRE(logs(events).at(0) == "Aria uses Second Wind: d10(6) + 2, regains 8 HP (now 11/12).");
    REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
    REQUIRE(encounter.character(1)->feature_uses_spent.at("SecondWind") == 1);
    REQUIRE_FALSE(encounter.bonus_action_available());
    REQUIRE_FALSE(offers(encounter, BonusAction::SecondWind));

    REQUIRE(encounter.can_action_surge());
    REQUIRE(encounter.action_surge());
    REQUIRE(encounter.turn().actions_left == 2);
    REQUIRE_FALSE(encounter.can_action_surge());  // once per turn (and per short rest)
    REQUIRE(encounter.dash());
    REQUIRE(encounter.dash());
    REQUIRE_FALSE(encounter.awaiting_action());
}

TEST_CASE("Rage: damage, resistance, and when it ends", "[encounter][class_features]") {
    Fixture f;

    SECTION("not in heavy armor") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(classed(f, 1, "Grog", "Barbarian"), {}, f.fighter_gear());  // chain mail
        encounter.add_monsters(f.monster("Goblin"), 1);
        encounter.roll_initiative();
        REQUIRE_FALSE(offers(encounter, BonusAction::Rage));
    }

    SECTION("a full rage") {
        // Grog first. Rage; Greataxe d20 15 hits, d12 5 (+3 +2 Rage = 10).
        // Goblin: pick, d20 15 hits AC 13, d6 4 (+2 = 6, halved to 3).
        // Grog dashes; Goblin misses (natural 1); Grog dashes again: the rage ends.
        ScriptedRandomSource rolls({20, 1, 1, 15, 5, 1, 15, 4, 1, 1, 1, 1});  // initiative: Grog, Goblin #1, #2
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(classed(f, 1, "Grog", "Barbarian"), {}, {{f.item("Greataxe"), 1, true}});
        encounter.add_monsters(f.monster("Goblin"), 2);
        encounter.roll_initiative();
        encounter.take_events();

        REQUIRE(encounter.use_bonus_action(BonusAction::Rage));
        REQUIRE(contains(logs(encounter.take_events()), "Grog flies into a rage!"));
        REQUIRE(encounter.status_text(0) == "Up (Raging)");
        REQUIRE(encounter.castable_spells().empty());
        REQUIRE(encounter.attack(1, 5, 1, 12, 3));
        REQUIRE(contains(logs(encounter.take_events()), "-> hit for 10 (+2 Rage). Goblin #1 is now 0/7 HP."));

        encounter.next_turn();  // Goblin #2 hits Grog, halved
        REQUIRE(contains(logs(encounter.take_events()), "-> hit for 3 (halved: resistant to slashing). Grog is now 9/12 HP."));

        encounter.dash();
        encounter.next_turn();  // he took damage since his last turn: the rage holds
        REQUIRE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::Raging));
        encounter.take_events();

        encounter.dash();
        encounter.next_turn();  // no attack, no damage this round: it ends
        REQUIRE(contains(logs(encounter.take_events()),
                         "Grog's rage ends (didn't attack or take damage this turn)."));
        REQUIRE_FALSE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::Raging));
    }
}

TEST_CASE("Sneak Attack needs advantage or an ally in the fight", "[encounter][class_features]") {
    Fixture f;
    auto rogue = classed(f, 1, "Vex", "Rogue");
    rogue.dexterity = 16;

    SECTION("with an ally") {
        // Vex 20+3, Zanna 1+2, Goblin 1+2. Rapier d20 15 (+5 = 20) hits, d8 3 (+3); Sneak Attack d6 4.
        ScriptedRandomSource rolls({20, 1, 1, 15, 3, 4});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(rogue, {}, {{f.item("Rapier"), 1, true}});
        encounter.add_character(f.wizard(2), {}, {});
        encounter.add_monsters(f.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.current()->name == "Vex");
        REQUIRE(encounter.attack(2, 5, 1, 8, 3));
        REQUIRE(contains(logs(encounter.take_events()),
                         "-> hit for 10 (+4 Sneak Attack, an ally is fighting the target). Goblin is now 0/7 HP."));
    }

    SECTION("alone, without advantage: no Sneak Attack") {
        ScriptedRandomSource rolls({20, 1, 15, 3});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(rogue, {}, {{f.item("Rapier"), 1, true}});
        encounter.add_monsters(f.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.attack(1, 5, 1, 8, 3));
        auto lines = logs(encounter.take_events());
        REQUIRE(contains(lines, "-> hit for 6. Goblin is now 1/7 HP."));
        REQUIRE_FALSE(contains(lines, "Sneak Attack"));
    }
}

TEST_CASE("Bardic Inspiration adds a die to an ally's next roll", "[encounter][class_features]") {
    Fixture f;
    auto bard = classed(f, 2, "Lyra", "Bard");
    bard.charisma = 16;
    bard.dexterity = 14;
    // Aria 1+1, Lyra 20+2, Goblin 1+2: Lyra, Goblin, Aria. Goblin picks Aria, rolls a 1.
    // Aria: Inspiration d6 5, d20 10 (+5 +5 = 20) hits, d10 1 (+3).
    ScriptedRandomSource rolls({1, 20, 1, 1, 1, 5, 10, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(1), {}, f.fighter_gear());
    encounter.add_character(bard, {}, {});
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current()->name == "Lyra");

    auto options = encounter.bonus_action_options();
    REQUIRE(options.size() == 1);
    REQUIRE(options[0].label == "Bardic Inspiration, d6 (3 left)");
    REQUIRE(encounter.bonus_action_targets(dnd::Encounter::BonusTarget::Ally) == std::vector<int>{0});
    REQUIRE(encounter.use_bonus_action(BonusAction::BardicInspiration, 0));
    REQUIRE(encounter.status_text(0) == "Up (Inspired (d6))");

    encounter.next_turn();  // Goblin, then Aria
    REQUIRE(encounter.current()->name == "Aria");
    encounter.take_events();
    REQUIRE(encounter.attack(2, 5, 1, 10, 3));
    REQUIRE(contains(logs(encounter.take_events()), "Aria attacks Goblin: d20(10) +5 +1d6(5) -> hit for 4."));
    REQUIRE_FALSE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::InspirationDie));
}

TEST_CASE("Martial Arts: a bonus unarmed strike after the Attack action", "[encounter][class_features]") {
    Fixture f;
    auto monk = classed(f, 1, "Kai", "Monk");
    monk.strength = 10;
    monk.dexterity = 16;
    // Kai first. Unarmed d20 15 (+5) hits, d4 2 (+3); bonus strike d20 15 hits, d4 2 (+3).
    ScriptedRandomSource rolls({20, 1, 15, 2, 15, 2});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(monk, {}, {});
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    const auto& kai = encounter.combatants()[0];
    REQUIRE(kai.attack_bonus == 5);
    REQUIRE(kai.damage_dice_sides == 4);
    REQUIRE(kai.martial_arts_strike.has_value());
    REQUIRE_FALSE(offers(encounter, BonusAction::MartialArtsStrike));  // not before the Attack action

    REQUIRE(encounter.attack(1, kai.attack_bonus, kai.damage_dice_count, kai.damage_dice_sides, kai.damage_bonus));
    REQUIRE(offers(encounter, BonusAction::MartialArtsStrike));
    REQUIRE(encounter.use_bonus_action(BonusAction::MartialArtsStrike, 1));
    auto lines = logs(encounter.take_events());
    REQUIRE(std::count_if(lines.begin(), lines.end(), [](const std::string& l) {
                return l.rfind("Kai attacks Orc: d20(15) +5 -> hit for 5.", 0) == 0;
            }) == 2);
}

TEST_CASE("Flurry of Blows and Patient Defense spend ki", "[encounter][class_features]") {
    Fixture f;
    auto monk = classed(f, 1, "Kai", "Monk", 2);
    monk.strength = 10;
    monk.dexterity = 16;

    SECTION("Flurry of Blows: two unarmed strikes after the Attack action") {
        // Kai first. Three hits on the Orc, d20 15 and d4 2 each (+3): 5 + 5 + 5.
        ScriptedRandomSource rolls({20, 1, 15, 2, 15, 2, 15, 2});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(monk, {}, {});
        encounter.add_monsters(f.monster("Orc"), 1);
        encounter.roll_initiative();
        encounter.take_events();

        REQUIRE_FALSE(offers(encounter, BonusAction::FlurryOfBlows));  // not before the Attack action
        REQUIRE(offers(encounter, BonusAction::PatientDefense));
        const auto& kai = encounter.combatants()[0];
        REQUIRE(encounter.attack(1, kai.attack_bonus, kai.damage_dice_count, kai.damage_dice_sides, kai.damage_bonus));
        REQUIRE(encounter.use_bonus_action(BonusAction::FlurryOfBlows, 1));
        auto lines = logs(encounter.take_events());
        REQUIRE(contains(lines, "Kai spends 1 ki on Flurry of Blows."));
        REQUIRE(std::count_if(lines.begin(), lines.end(), [](const std::string& l) {
                    return l.rfind("Kai attacks Orc: d20(15) +5 -> hit for 5.", 0) == 0;
                }) == 3);
        REQUIRE(encounter.character(1)->feature_uses_spent.at("Ki") == 1);
    }

    SECTION("Patient Defense: Dodge as a bonus action") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(monk, {}, {});
        encounter.add_monsters(f.monster("Orc"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.use_bonus_action(BonusAction::PatientDefense));
        REQUIRE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::Dodging));
        REQUIRE(encounter.awaiting_action());  // the action is still free
        REQUIRE(encounter.character(1)->feature_uses_spent.at("Ki") == 1);
    }

    SECTION("no ki left, no ki options") {
        monk.feature_uses_spent["Ki"] = 2;
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(monk, {}, {});
        encounter.add_monsters(f.monster("Orc"), 1);
        encounter.roll_initiative();
        REQUIRE_FALSE(offers(encounter, BonusAction::PatientDefense));
        REQUIRE_FALSE(offers(encounter, BonusAction::StepOfTheWindDash));
    }
}

TEST_CASE("Divine Smite spends a slot on a hit", "[encounter][class_features]") {
    Fixture f;
    auto paladin = classed(f, 1, "Sera", "Paladin", 2);  // two 1st-level slots

    SECTION("a hit") {
        // Sera first. d20 15 hits the Orc; longsword two-handed d10 5 (+3); smite 2d8: 4 + 4.
        ScriptedRandomSource rolls({20, 1, 15, 5, 4, 4});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(paladin, {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Orc"), 1);
        encounter.roll_initiative();
        encounter.take_events();

        REQUIRE(encounter.divine_smite_slots() == std::vector<int>{1});
        const auto& sera = encounter.combatants()[0];
        dnd::AttackOptions smite;
        smite.divine_smite_slot = 2;
        REQUIRE_FALSE(encounter.attack(1, sera.attack_bonus, 1, 10, 3, smite));  // no 2nd-level slots
        smite.divine_smite_slot = 1;
        REQUIRE(encounter.attack(1, sera.attack_bonus, 1, 10, 3, smite));
        auto events = encounter.take_events();
        REQUIRE(contains(logs(events), "-> hit for 16 (+8 radiant, Divine Smite 2d8, level 1 slot)."));
        REQUIRE(encounter.character(1)->spell_slots_used[0] == 1);
        REQUIRE(has_event(events, EncounterEvent::Kind::CharacterChanged, 1));
    }

    SECTION("a miss spends nothing") {
        ScriptedRandomSource rolls({20, 1, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(paladin, {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Orc"), 1);
        encounter.roll_initiative();
        dnd::AttackOptions smite;
        smite.divine_smite_slot = 1;
        REQUIRE(encounter.attack(1, 5, 1, 10, 3, smite));
        REQUIRE(encounter.character(1)->spell_slots_used[0] == 0);
    }
}

TEST_CASE("Lay on Hands heals from a pool", "[encounter][class_features]") {
    Fixture f;
    auto aria = f.fighter(1);
    aria.current_hp = 3;
    // Sera 20, Aria 1, Goblin 1: Sera first.
    ScriptedRandomSource rolls({1, 20, 1});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(aria, {}, f.fighter_gear());
    encounter.add_character(classed(f, 2, "Sera", "Paladin"), {}, f.fighter_gear());
    encounter.add_monsters(f.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.current()->name == "Sera");

    REQUIRE(encounter.lay_on_hands_pool() == 5);
    REQUIRE(encounter.lay_on_hands_targets() == std::vector<int>{0, 1});
    REQUIRE_FALSE(encounter.lay_on_hands(0, 6));  // more than the pool
    REQUIRE(encounter.lay_on_hands(0, 4));
    REQUIRE(contains(logs(encounter.take_events()),
                     "Sera uses Lay on Hands on Aria: 4 from the pool, 4 HP restored (now 7/12; 1 left in the pool)."));
    REQUIRE(encounter.character(1)->current_hp == 7);
    REQUIRE(encounter.character(2)->feature_uses_spent.at("LayOnHands") == 4);
    REQUIRE_FALSE(encounter.awaiting_action());  // it took the action
}

TEST_CASE("Reckless Attack: advantage both ways", "[encounter][class_features]") {
    Fixture f;
    // Grog first. Reckless: d20 3 and 15 with advantage, d12 5 (+3).
    // Orc: pick, d20 4 and 16 with advantage (+5 = 21) hits, d12 2 (+3).
    ScriptedRandomSource rolls({20, 1, 3, 15, 5, 1, 4, 16, 2});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(classed(f, 1, "Grog", "Barbarian", 2), {}, {{f.item("Greataxe"), 1, true}});
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();
    encounter.take_events();

    REQUIRE(encounter.can_attack_recklessly());
    dnd::AttackOptions reckless;
    reckless.reckless = true;
    REQUIRE(encounter.attack(1, 5, 1, 12, 3, reckless));
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Grog attacks recklessly"));
    REQUIRE(contains(lines, "Grog attacks Orc: d20(15)"));
    REQUIRE(contains(lines, "Reckless Attack"));
    REQUIRE_FALSE(encounter.can_attack_recklessly());

    encounter.next_turn();  // the Orc attacks with advantage; then Grog's turn ends the exposure
    lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Orc attacks Grog: d20(16)"));
    REQUIRE_FALSE(dnd::has_effect(encounter.effects(), 0, dnd::EffectKind::AdvantageOnAttacksAgainst));
    REQUIRE(encounter.can_attack_recklessly());  // a new turn, a new choice
}

TEST_CASE("Great Weapon Fighting in a fight", "[encounter][class_features]") {
    Fixture f;
    auto aria = f.fighter(1);
    aria.fighting_style = "GreatWeaponFighting";
    // Aria first. d20 15 hits; 2d6 1 and 5, the 1 rerolled to 6: 6 + 5 + 3 = 14.
    ScriptedRandomSource rolls({20, 1, 15, 1, 5, 6});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(aria, {}, {{f.item("Greatsword"), 1, true}});
    encounter.add_monsters(f.monster("Orc"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    REQUIRE(encounter.combatants()[0].attack_great_weapon_fighting);
    REQUIRE(encounter.attack(1, 5, 2, 6, 3));
    REQUIRE(contains(logs(encounter.take_events()),
                     "-> hit for 14 (Great Weapon Fighting rerolled 1 die). Orc is now 1/15 HP."));
}

TEST_CASE("damage types: vulnerability, immunity, and Rage as resistance", "[encounter][damage]") {
    Fixture f;

    SECTION("a mace doubles against a skeleton") {
        // Aria first. d20 15 hits AC 13; d6 3 (+3) = 6, doubled.
        ScriptedRandomSource rolls({20, 1, 15, 3});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(f.fighter(), {}, {{f.item("Mace"), 1, true}});
        encounter.add_monsters(f.monster("Skeleton"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.attack(1, 5, 1, 6, 3));
        REQUIRE(contains(logs(encounter.take_events()),
                         "-> hit for 12 (doubled: vulnerable to bludgeoning). Skeleton is now 1/13 HP."));
    }

    SECTION("poison does nothing to a zombie, which can't be poisoned either") {
        // Zanna first. Poison Spray: the Zombie's CON save d20 1 fails; d12 8.
        ScriptedRandomSource rolls({20, 1, 1, 8});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        auto spray = f.spell("Poison Spray");
        encounter.add_character(f.wizard(1), {spray}, {});
        encounter.add_monsters(f.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.cast_spell(spray.spell_id, 0, {1}));
        REQUIRE(contains(logs(encounter.take_events()), "takes 0 (immune to poison). Now 22/22 HP."));

        dnd::ActiveEffect poisoned;
        poisoned.kind = dnd::EffectKind::Condition;
        poisoned.condition = dnd::Condition::Poisoned;
        poisoned.target = 1;
        REQUIRE(encounter.add_effect(poisoned) == 0);
        REQUIRE(contains(logs(encounter.take_events()), "Zombie is immune to being poisoned."));
        REQUIRE_FALSE(dnd::has_condition(encounter.effects(), 1, dnd::Condition::Poisoned));
    }

    SECTION("Rage resists bludgeoning, piercing, and slashing only") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(classed(f, 1, "Grog", "Barbarian"), {}, {{f.item("Greataxe"), 1, true}});
        encounter.add_monsters(f.monster("Goblin"), 1);
        encounter.roll_initiative();
        REQUIRE(encounter.damage_traits(0).resistances.empty());
        REQUIRE(encounter.use_bonus_action(BonusAction::Rage));
        auto resists = encounter.damage_traits(0).resistances;
        REQUIRE(resists == std::vector<std::string>{"bludgeoning", "piercing", "slashing"});
        REQUIRE(dnd::damage_modifier(encounter.damage_traits(0), "fire") == dnd::DamageModifier::Normal);
    }
}

TEST_CASE("creature types: Charm Person and Divine Smite", "[encounter][damage]") {
    Fixture f;

    SECTION("Charm Person targets humanoids only") {
        ScriptedRandomSource rolls({20, 1, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        auto charm = f.spell("Charm Person");
        encounter.add_character(f.wizard(1), {charm}, {});
        encounter.add_monsters(f.monster("Goblin"), 1);
        encounter.add_monsters(f.monster("Skeleton"), 1);
        encounter.roll_initiative();
        REQUIRE(encounter.spell_target_candidates(charm) == std::vector<int>{1});  // not the Skeleton (2)
    }

    SECTION("Divine Smite adds a die against undead") {
        // Sera first. d20 15 hits the Skeleton; longsword two-handed d10 5 (+3); smite 3d8: 1 + 1 + 1.
        ScriptedRandomSource rolls({20, 1, 15, 5, 1, 1, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(classed(f, 1, "Sera", "Paladin", 2), {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Skeleton"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        dnd::AttackOptions smite;
        smite.divine_smite_slot = 1;
        REQUIRE(encounter.attack(1, 5, 1, 10, 3, smite));
        REQUIRE(contains(logs(encounter.take_events()),
                         "-> hit for 11 (+3 radiant, Divine Smite 3d8 vs undead, level 1 slot). Skeleton is now 2/13 HP."));
    }
}

TEST_CASE("Multiattack: every attack, retargeting when a target drops", "[encounter][monsters]") {
    Fixture f;
    // Init: Aria 1, Zanna 1, Bandit Captain 20 (first). Pick Aria (1 of 2 -> 1).
    // Scimitar d20 15 hits (+5 vs AC 16), d6 6 (+3) = 9 -> Aria 3/12. Second scimitar: hits, 9 -> Aria down.
    // Dagger: retargets Zanna (pick 1 of 1), d20 15 hits AC 12, d4 4 (+3) = 7 -> Zanna 1/8.
    ScriptedRandomSource rolls({1, 1, 20, 1, 15, 6, 15, 6, 1, 15, 4});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    encounter.add_character(f.fighter(1), {}, f.fighter_gear());
    encounter.add_character(f.wizard(2), {}, {});
    encounter.add_monsters(f.monster("Bandit Captain"), 1);
    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Bandit Captain uses Multiattack: Two scimitar attacks and a dagger attack."));
    REQUIRE(std::count_if(lines.begin(), lines.end(), [](const std::string& l) {
                return l.rfind("Bandit Captain attacks Aria", 0) == 0;
            }) == 2);
    REQUIRE(contains(lines, "Bandit Captain attacks Zanna: d20(15) +5 -> hit for 7. Zanna is now 1/8 HP."));
}

TEST_CASE("monster traits", "[encounter][monsters]") {
    Fixture f;

    SECTION("Pack Tactics: advantage while an ally is fighting") {
        // Aria 1; Kobolds 20 and 20. Kobold #1: pick, d20 2 and 18 with advantage (+4) hits AC 16.
        ScriptedRandomSource rolls({1, 20, 20, 1, 2, 18, 3});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(f.fighter(1), {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Kobold"), 2);
        encounter.roll_initiative();
        REQUIRE(contains(logs(encounter.take_events()), "Kobold #1 attacks Aria: d20(18) +4 with advantage (Pack Tactics)"));
    }

    SECTION("Martial Advantage: +2d6 once per turn with an ally fighting") {
        // Aria 1; Hobgoblins 20, 20. #1: pick, d20 15 (+3) hits AC 16, d8 4 (+1), 2d6 3 + 3.
        ScriptedRandomSource rolls({1, 20, 20, 1, 15, 4, 3, 3, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(f.fighter(1), {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Hobgoblin"), 2);
        encounter.roll_initiative();
        REQUIRE(contains(logs(encounter.take_events()),
                         "Hobgoblin #1 attacks Aria: d20(15) +3 -> hit for 11 (+6 Martial Advantage). Aria is now 1/12 HP."));
    }

    SECTION("Undead Fortitude: a zombie can stay up at 1 HP") {
        // Aria (level 5, Extra Attack) first. Two hits for 13 each (d20 15, d10 10 +3): the second would
        // drop the Zombie (9 HP), so it makes a CON save, DC 5 + 13 = 18: d20 20 (+3) succeeds.
        ScriptedRandomSource rolls({20, 1, 15, 10, 15, 10, 20});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(classed(f, 1, "Aria", "Fighter", 5), {}, f.fighter_gear());  // Extra Attack
        encounter.add_monsters(f.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.attack(1, 5, 1, 10, 3));
        REQUIRE(encounter.combatants()[1].current_hp == 9);
        encounter.take_events();
        REQUIRE(encounter.attack(1, 5, 1, 10, 3));
        auto lines = logs(encounter.take_events());
        REQUIRE(contains(lines, "Undead Fortitude: Zombie CON save d20(20) +3 vs DC 18 -> stays up with 1 HP!"));
        REQUIRE(encounter.combatants()[1].current_hp == 1);
    }

    SECTION("Regeneration, stopped by fire") {
        ScriptedRandomSource rolls({1, 20, 20});  // Zanna 1, Troll 20; then 20s: Fire Bolt crits
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(f.wizard(1), {f.spell("Fire Bolt")}, {});
        encounter.add_monsters(f.monster("Troll"), 1);
        auto& troll = const_cast<dnd::Combatant&>(encounter.combatants()[1]);
        troll.current_hp = 50;
        troll.monster_attacks.clear();  // keep the wizard alive for the test
        encounter.roll_initiative();    // Troll first: regenerates to 60
        auto lines = logs(encounter.take_events());
        REQUIRE(contains(lines, "Troll regenerates 10 HP (now 60/84)."));
        // Fire Bolt hits (d20 20 crit): fire damage blocks the next regeneration.
        REQUIRE(encounter.cast_spell(f.spell("Fire Bolt").spell_id, 0, {1}));
        encounter.next_turn();
        auto after = logs(encounter.take_events());
        REQUIRE(contains(after, "Troll doesn't regenerate (fire or acid)."));
    }

    SECTION("a Ghoul's claws paralyze, with a save each turn") {
        // Aria 1, Ghoul 20. Claws d20 15 hits AC 16? +4 = 19 hits; 2d4 2 + 2 (+2) = 6; CON save d20 1 fails.
        ScriptedRandomSource rolls({1, 20, 1, 15, 2, 2, 1});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(f.fighter(1), {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Ghoul"), 1);
        encounter.roll_initiative();
        REQUIRE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Paralyzed));
        auto paralysis = std::find_if(encounter.effects().begin(), encounter.effects().end(),
                                      [](const dnd::ActiveEffect& e) { return e.condition == dnd::Condition::Paralyzed; });
        REQUIRE(paralysis->save_to_end.has_value());
        REQUIRE(paralysis->save_to_end->dc == 10);
        REQUIRE(paralysis->duration.rounds == 10);
    }

    SECTION("Hell Hound: fire on the bite, and immune to fire") {
        ScriptedRandomSource rolls({1, 20, 1, 15, 4, 3, 3});
        dnd::Dice dice(rolls);
        auto encounter = f.encounter(dice);
        encounter.add_character(f.fighter(1), {}, f.fighter_gear());
        encounter.add_monsters(f.monster("Hell Hound"), 1);
        encounter.roll_initiative();
        // Bite d8 4 (+3) = 7 piercing, + 2d6 (3 + 3) fire = 13.
        REQUIRE(contains(logs(encounter.take_events()), "-> hit for 13 (+6 fire). Aria is now 0/12 HP."));
        REQUIRE(dnd::damage_modifier(encounter.damage_traits(1), "fire") == dnd::DamageModifier::Immune);
    }
}
