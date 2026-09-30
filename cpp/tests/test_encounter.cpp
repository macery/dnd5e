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

TEST_CASE("joining at 0 HP means unconscious but stable", "[encounter]") {
    Fixture f;
    ScriptedRandomSource rolls({10});
    dnd::Dice dice(rolls);
    auto encounter = f.encounter(dice);
    auto aria = f.fighter();
    aria.current_hp = 0;
    encounter.add_character(aria, {}, {});
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
    encounter.add_character(aria, {}, {});
    encounter.add_character(f.wizard(), {}, {});
    encounter.add_monsters(f.monster("Orc"), 1);

    encounter.roll_initiative();
    auto lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Orc attacks Aria: d20(15) +5 -> hit for 8. Aria is now 0/12 HP."));
    REQUIRE(contains(lines, "    Aria falls unconscious and is dying."));
    REQUIRE(encounter.status_text(0) == "Dying (✓0 ✗0)");
    REQUIRE(dnd::has_condition(encounter.effects(), 0, dnd::Condition::Unconscious));
    REQUIRE(encounter.current()->name == "Zanna");

    REQUIRE(encounter.dash());
    encounter.next_turn();  // Aria rolls a death save, then the Orc goes for Zanna, not the dying Aria
    lines = logs(encounter.take_events());
    REQUIRE(contains(lines, "Aria makes a death saving throw: d20(9) -> failure (0 successes, 1 failures)."));
    REQUIRE(contains(lines, "Orc attacks Zanna"));
    REQUIRE(encounter.status_text(0) == "Dying (✓0 ✗1)");

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
    encounter.add_character(aria, {}, {});
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
    encounter.add_character(aria, {}, {});
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
    encounter.add_character(aria, {}, {});
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
    encounter.add_character(f.fighter(), {}, {});
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
    encounter.add_character(f.fighter(), {}, {});
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
