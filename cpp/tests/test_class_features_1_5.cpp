#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/class_features.hpp"
#include "dnd/class_options.hpp"
#include "dnd/encounter.hpp"
#include "dnd/equipment.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"
#include "dnd/spellbook.hpp"

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

struct Party {
    dnd::Repository repo{":memory:"};

    dnd::Character hero(const std::string& class_name, int level, int id = 1) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = class_name;
        for (const auto& entry : repo.get_classes()) {
            if (entry.name == class_name) c.class_id = entry.id;
        }
        c.level = level;
        c.strength = 16;
        c.dexterity = 16;
        c.constitution = 14;
        c.wisdom = 16;
        c.charisma = 16;
        c.max_hp = 40;
        c.current_hp = 40;
        return c;
    }

    dnd::Monster monster(const std::string& name) {
        for (const auto& m : repo.get_monsters()) {
            if (m.name == name) return m;
        }
        FAIL("no monster " << name);
        return {};
    }

    dnd::Spell spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return {};
    }

    dnd::Encounter fight(dnd::Dice& dice) { return dnd::Encounter(dice, repo.get_classes(), repo.get_races()); }
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

TEST_CASE("Bard: Jack of All Trades and Song of Rest", "[class_features_1_5]") {
    Party p;
    REQUIRE(dnd::jack_of_all_trades(p.hero("Bard", 1), "Bard") == 0);
    REQUIRE(dnd::jack_of_all_trades(p.hero("Bard", 2), "Bard") == 1);
    REQUIRE(dnd::jack_of_all_trades(p.hero("Bard", 5), "Bard") == 1);  // proficiency 3 -> 1
    REQUIRE(dnd::jack_of_all_trades(p.hero("Fighter", 5), "Fighter") == 0);
    REQUIRE(dnd::song_of_rest_die(1) == 0);
    REQUIRE(dnd::song_of_rest_die(2) == 6);
    REQUIRE(dnd::song_of_rest_die(9) == 8);

    SECTION("Song of Rest adds a die when hit dice are spent") {
        ScriptedRandomSource rolls({5, 4});  // a d10 hit die, then the d6 song
        dnd::Dice dice(rolls);
        auto fighter = p.hero("Fighter", 3);
        fighter.current_hp = 10;
        auto result = dnd::short_rest(fighter, *dnd::find_class_info("Fighter"), dice, 1, 6);
        REQUIRE(result.song_of_rest == 4);
        REQUIRE(fighter.current_hp == 10 + 5 + 2 + 4);
        auto rested = p.hero("Fighter", 3);
        REQUIRE(dnd::short_rest(rested, *dnd::find_class_info("Fighter"), dice, 0, 6).song_of_rest == 0);
    }
    SECTION("initiative includes it") {
        ScriptedRandomSource rolls({10});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        encounter.add_character(p.hero("Bard", 2), {}, {});
        encounter.add_monsters(p.monster("Goblin"), 1);
        encounter.roll_initiative();
        REQUIRE(encounter.combatants()[0].initiative == 10 + 3 + 1);  // DEX +3, Jack of All Trades +1
    }
}

TEST_CASE("Sorcerer: Font of Magic", "[class_features_1_5]") {
    Party p;
    auto sorcerer = p.hero("Sorcerer", 3);  // 3 points; slots 4 x 1st, 2 x 2nd
    REQUIRE(dnd::feature_max_uses(dnd::Feature::SorceryPoints, sorcerer, "Sorcerer") == 3);
    REQUIRE(dnd::feature_max_uses(dnd::Feature::SorceryPoints, p.hero("Sorcerer", 1), "Sorcerer") == 0);

    REQUIRE(dnd::create_spell_slot(sorcerer, "Sorcerer", 1));  // 2 points
    REQUIRE(dnd::slots_remaining(sorcerer, *dnd::find_class_info("Sorcerer"), 1) == 5);
    REQUIRE_FALSE(dnd::create_spell_slot(sorcerer, "Sorcerer", 2));  // 3 points, only 1 left
    REQUIRE(dnd::convert_slot_to_points(sorcerer, "Sorcerer", 2));    // +2, capped at 3
    REQUIRE(dnd::feature_uses_left(dnd::Feature::SorceryPoints, sorcerer, "Sorcerer") == 3);
    REQUIRE_FALSE(dnd::convert_slot_to_points(sorcerer, "Sorcerer", 1));  // already full

    dnd::take_long_rest(sorcerer);  // created slots are gone, points are back
    REQUIRE(dnd::slots_remaining(sorcerer, *dnd::find_class_info("Sorcerer"), 1) == 4);
    REQUIRE(dnd::slot_creation_cost(3) == 5);
    REQUIRE(dnd::slot_creation_cost(6) == 0);
}

TEST_CASE("Sorcerer: Metamagic", "[class_features_1_5]") {
    Party p;
    REQUIRE(dnd::metamagic_cost(dnd::Metamagic::Twinned, 0) == 1);
    REQUIRE(dnd::metamagic_cost(dnd::Metamagic::Twinned, 3) == 3);
    REQUIRE(dnd::metamagic_cost(dnd::Metamagic::Heightened, 1) == 3);
    REQUIRE(dnd::metamagic_known(3) == 2);
    REQUIRE(dnd::metamagic_from_name("Quickened Spell") == dnd::Metamagic::Quickened);

    auto sorcerer = p.hero("Sorcerer", 5);
    sorcerer.class_options["Metamagic"] = {"Quickened Spell", "Twinned Spell"};
    auto setup = [&](dnd::Encounter& encounter, int goblins) {
        encounter.add_character(sorcerer, {p.spell("Fire Bolt"), p.spell("Magic Missile")}, {});
        encounter.add_monsters(p.monster("Goblin"), goblins);
        encounter.roll_initiative();
        encounter.take_events();
    };

    SECTION("options fit the spell") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        setup(encounter, 2);
        auto options = encounter.metamagic_options(p.spell("Fire Bolt"), 0);
        REQUIRE(options.size() == 2);  // Quickened (2), Twinned (1)
        auto missile = encounter.metamagic_options(p.spell("Magic Missile"), 1);
        REQUIRE(missile.size() == 1);  // Magic Missile hits several targets: not Twinned
    }
    SECTION("Twinned Fire Bolt hits two goblins") {
        ScriptedRandomSource rolls({20, 1, 1, 15});  // then 15s: hits, and 10s on the d10s
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        setup(encounter, 2);
        REQUIRE(encounter.cast_spell(p.spell("Fire Bolt").spell_id, 0, {1, 2}, dnd::Metamagic::Twinned));
        auto text = text_of(encounter.take_events());
        REQUIRE(has(text, "with Twinned Spell (1 sorcery point)"));
        REQUIRE(encounter.combatants()[1].current_hp <= 0);
        REQUIRE(encounter.combatants()[2].current_hp <= 0);
        REQUIRE(dnd::feature_uses_left(dnd::Feature::SorceryPoints, *encounter.character(1), "Sorcerer") == 4);
    }
    SECTION("Quickened uses the bonus action, leaving the action") {
        ScriptedRandomSource rolls({20, 1, 1, 15, 3});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        setup(encounter, 1);
        REQUIRE(encounter.cast_spell(p.spell("Fire Bolt").spell_id, 0, {1}, dnd::Metamagic::Quickened));
        REQUIRE(encounter.turn().bonus_action_used);
        REQUIRE(encounter.turn().actions_left == 1);
    }
    SECTION("not without the option or the points") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        setup(encounter, 1);
        REQUIRE_FALSE(encounter.cast_spell(p.spell("Fire Bolt").spell_id, 0, {1}, dnd::Metamagic::Heightened));
    }
}

TEST_CASE("Font of Magic as a bonus action", "[class_features_1_5]") {
    Party p;
    ScriptedRandomSource rolls({20, 1});
    dnd::Dice dice(rolls);
    auto encounter = p.fight(dice);
    encounter.add_character(p.hero("Sorcerer", 3), {}, {});
    encounter.add_monsters(p.monster("Goblin"), 1);
    encounter.roll_initiative();
    encounter.take_events();
    auto levels = encounter.bonus_action_targets(dnd::Encounter::BonusTarget::SlotLevel);
    REQUIRE(levels == std::vector<int>{1, 2});  // 3 points: 2 or 3
    REQUIRE(encounter.use_bonus_action(dnd::Encounter::BonusAction::CreateSpellSlot, 2));
    REQUIRE(has(text_of(encounter.take_events()), "3 sorcery points become a 2nd-level spell slot"));
    REQUIRE(encounter.character(1)->spell_slots_used[1] == -1);
}

TEST_CASE("Barbarian Danger Sense, Monk Stunning Strike and Deflect Missiles", "[class_features_1_5]") {
    Party p;

    SECTION("Danger Sense: advantage on DEX saves") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        encounter.add_character(p.hero("Barbarian", 2), {}, {});
        encounter.add_monsters(p.monster("Goblin"), 1);
        encounter.roll_initiative();
        auto dex = encounter.save_modifiers(0, dnd::Ability::Dexterity);
        REQUIRE(std::find(dex.advantage.begin(), dex.advantage.end(), "Danger Sense") != dex.advantage.end());
        REQUIRE(encounter.save_modifiers(0, dnd::Ability::Strength).advantage.empty());
    }
    SECTION("Stunning Strike: ki on a hit, CON save or stunned") {
        // Initiative: monk 20, zombie 1. Attack d20 15 hits, 1d6 3; the zombie's CON save d20 1.
        ScriptedRandomSource rolls({20, 1, 15, 3, 1});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        encounter.add_character(p.hero("Monk", 5), {}, {});
        encounter.add_monsters(p.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.can_stunning_strike());
        const auto& me = encounter.combatants()[0];
        dnd::AttackOptions options;
        options.stunning_strike = true;
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus, options);
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "Stunning Strike (1 ki): Zombie"));
        REQUIRE(has(text, "-> stunned until the end of Monk's next turn."));
        REQUIRE(dnd::has_condition(encounter.effects(), 1, dnd::Condition::Stunned));
        REQUIRE(dnd::feature_uses_left(dnd::Feature::Ki, *encounter.character(1), "Monk") == 4);
    }
    SECTION("Deflect Missiles: a ranged hit loses 1d10 + DEX + level") {
        dnd::Monster archer = p.monster("Goblin");
        archer.name = "Archer";
        archer.attacks.front().name = "Shortbow";
        archer.attacks.front().ranged = true;
        // Initiative: monk 1, archer 20. Shortbow d20 19 hits, 1d6 6 + 2 = 8; Deflect d10 10 + 3 + 3 = 16.
        ScriptedRandomSource rolls({1, 20, 1, 19, 6, 10});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        encounter.add_character(p.hero("Monk", 3), {}, {});
        encounter.add_monsters(archer, 1);
        encounter.roll_initiative();
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "(Deflect Missiles: -16)"));
        REQUIRE(encounter.combatants()[0].current_hp == 40);
    }
}

TEST_CASE("class choices: what's due, and choosing", "[class_features_1_5][class_options]") {
    Party p;
    dnd::SpellContext with_blast{{"Eldritch Blast", "Fire Bolt", "Light"}, {"Eldritch Blast"}};

    SECTION("a sorcerer's Metamagic at 3") {
        auto sorcerer = p.hero("Sorcerer", 3);
        auto due = dnd::class_choices_due(sorcerer, "Sorcerer");
        REQUIRE(due.size() == 1);
        REQUIRE(due[0].key == "Metamagic");
        REQUIRE(due[0].count == 2);
        REQUIRE(dnd::apply_class_choice(sorcerer, "Sorcerer", "Metamagic", {"Quickened Spell"}) ==
                "Choose 2 for Metamagic.");
        REQUIRE(dnd::apply_class_choice(sorcerer, "Sorcerer", "Metamagic", {"Quickened Spell", "Twinned Spell"}).empty());
        REQUIRE(dnd::class_choices_due(sorcerer, "Sorcerer").empty());
    }
    SECTION("a warlock's invocations need Eldritch Blast for Agonizing Blast") {
        auto warlock = p.hero("Warlock", 2);
        auto without = dnd::class_choices_due(warlock, "Warlock");
        REQUIRE(without.at(0).key == "Invocations");
        REQUIRE(std::find(without[0].options.begin(), without[0].options.end(), "Agonizing Blast") ==
                without[0].options.end());
        auto with = dnd::class_choices_due(warlock, "Warlock", with_blast);
        REQUIRE(std::find(with[0].options.begin(), with[0].options.end(), "Agonizing Blast") != with[0].options.end());
        REQUIRE(dnd::apply_class_choice(warlock, "Warlock", "Invocations", {"Agonizing Blast", "Armor of Shadows"},
                                        with_blast)
                    .empty());
        REQUIRE(dnd::has_agonizing_blast(warlock));
        REQUIRE(dnd::calculate_armor_class(warlock, "Warlock", {}).total == 16);  // 13 + DEX 3
    }
    SECTION("Pact of the Blade at 3, Thirsting Blade at 5") {
        auto warlock = p.hero("Warlock", 5);
        warlock.class_options["Invocations"] = {"Armor of Shadows", "Fiendish Vigor"};
        auto due = dnd::class_choices_due(warlock, "Warlock");
        REQUIRE(due.at(0).key == "PactBoon");
        REQUIRE(dnd::apply_class_choice(warlock, "Warlock", "PactBoon", {"Pact of the Blade"}).empty());
        REQUIRE(dnd::apply_class_choice(warlock, "Warlock", "Invocations", {"Thirsting Blade"}).empty());
        REQUIRE(dnd::has_thirsting_blade(warlock));
        // The pact weapon: proficient with a greataxe, and two attacks.
        std::vector<dnd::InventoryEntry> axe;
        for (const auto& item : p.repo.get_items()) {
            if (item.name == "Greataxe") axe.push_back({item, 1, true});
        }
        REQUIRE(dnd::weapon_attack(warlock, "Warlock", "Human", axe).proficient);
        ScriptedRandomSource rolls({20});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        encounter.add_character(warlock, {}, axe);
        REQUIRE(encounter.combatants()[0].attacks_per_action == 2);
    }
    SECTION("Pact of the Tome cantrips are always ready") {
        auto warlock = p.hero("Warlock", 3);
        warlock.player_id = p.repo.add_player("Adam");
        warlock.race = p.repo.get_races().front().id;
        warlock.alignment = p.repo.get_alignments().front().id;
        warlock.class_options["PactBoon"] = {"Pact of the Tome"};
        auto due = dnd::class_choices_due(warlock, "Warlock", with_blast);
        auto tome = std::find_if(due.begin(), due.end(), [](const dnd::ClassChoice& c) { return c.key == "TomeCantrips"; });
        REQUIRE(tome != due.end());
        REQUIRE(tome->count == 3);
        REQUIRE(dnd::apply_class_choice(warlock, "Warlock", "TomeCantrips", {"Fire Bolt", "Light", "Eldritch Blast"},
                                        with_blast)
                    .empty());
        warlock.character_id = p.repo.add_character(warlock);
        auto ready = dnd::spells_ready(p.repo, warlock);
        REQUIRE(std::any_of(ready.begin(), ready.end(), [](const dnd::Spell& s) { return s.name == "Fire Bolt"; }));
    }
    SECTION("Expertise doubles proficiency; Lore bards' bonus skills") {
        auto rogue = p.hero("Rogue", 1);
        rogue.skill_proficiencies = (1u << static_cast<int>(dnd::Skill::Stealth)) |
                                    (1u << static_cast<int>(dnd::Skill::Perception));
        int before = dnd::skill_bonus(rogue, dnd::Skill::Stealth);
        REQUIRE(dnd::apply_class_choice(rogue, "Rogue", "Expertise", {"Stealth", "Perception"}).empty());
        REQUIRE(dnd::skill_bonus(rogue, dnd::Skill::Stealth) == before + 2);
        REQUIRE(dnd::class_choices_due(rogue, "Rogue").empty());

        auto bard = p.hero("Bard", 3);
        bard.subclass = "College of Lore";
        auto due = dnd::class_choices_due(bard, "Bard");
        auto bonus = std::find_if(due.begin(), due.end(), [](const dnd::ClassChoice& c) { return c.key == "BonusSkills"; });
        REQUIRE(bonus != due.end());
        REQUIRE(dnd::apply_class_choice(bard, "Bard", "BonusSkills", {"Arcana", "History", "Nature"}).empty());
        REQUIRE(dnd::has_skill_proficiency(bard, dnd::Skill::Arcana));
    }
    SECTION("Agonizing Blast adds CHA to each beam") {
        auto warlock = p.hero("Warlock", 2);
        warlock.class_options["Invocations"] = {"Agonizing Blast", "Armor of Shadows"};
        ScriptedRandomSource rolls({20, 1, 15, 5});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        encounter.add_character(warlock, {p.spell("Eldritch Blast")}, {});
        encounter.add_monsters(p.monster("Ogre"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.cast_spell(p.spell("Eldritch Blast").spell_id, 0, {1}));
        REQUIRE(encounter.combatants()[1].current_hp == 59 - (5 + 3));  // 1d10 5 + CHA 3
    }
}

TEST_CASE("Druid: Wild Shape", "[class_features_1_5][wild_shape]") {
    Party p;
    auto setup = [&](dnd::Encounter& encounter, int level, const std::string& foe = "Goblin") {
        auto druid = p.hero("Druid", level);
        druid.max_hp = 20;
        druid.current_hp = 20;
        encounter.set_wild_shape_beasts(p.repo.get_monsters());
        encounter.add_character(druid, {p.spell("Produce Flame")}, {});
        encounter.add_monsters(p.monster(foe), 1);
        encounter.roll_initiative();
        encounter.take_events();
    };
    auto names = [](const std::vector<dnd::Monster>& forms) {
        std::vector<std::string> list;
        for (const auto& m : forms) list.push_back(m.name);
        std::sort(list.begin(), list.end());
        return list;
    };

    SECTION("forms by level") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        auto two = p.fight(dice);
        setup(two, 2);
        REQUIRE(names(two.wild_shape_forms()) ==
                std::vector<std::string>{"Boar", "Giant Badger", "Panther", "Wolf"});
        auto four = p.fight(dice);
        setup(four, 4);
        auto forms = names(four.wild_shape_forms());
        REQUIRE(std::find(forms.begin(), forms.end(), "Black Bear") != forms.end());
        REQUIRE(std::find(forms.begin(), forms.end(), "Dire Wolf") == forms.end());  // CR 1
    }
    SECTION("becoming a black bear, and the damage carrying over") {
        // Druid first; Wild Shape. Then the ogre: target 1, d20 19 hits, 2d8 = 8 + 8 (+4).
        ScriptedRandomSource rolls({20, 1, 1, 19, 8});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        setup(encounter, 4, "Ogre");
        REQUIRE(encounter.wild_shape("Black Bear"));
        const auto& bear = encounter.combatants()[0];
        REQUIRE(bear.druid_form.has_value());
        REQUIRE(bear.current_hp == 19);
        REQUIRE(bear.armor_class == 11);
        REQUIRE_FALSE(encounter.awaiting_action());
        REQUIRE(encounter.castable_spells().empty());
        REQUIRE(dnd::feature_uses_left(dnd::Feature::WildShape, *encounter.character(1), "Druid") == 1);
        auto text = text_of(encounter.take_events());
        REQUIRE(has(text, "uses Wild Shape and becomes a Black Bear (19 HP, AC 11)"));
        REQUIRE(encounter.character(1)->current_hp == 20);  // the druid's own HP, kept aside

        // The ogre's turn: its greatclub (2d8 + 4 = 20) drops the bear's 19 HP; 1 carries over.
        encounter.next_turn();
        auto hit = text_of(encounter.take_events());
        INFO(hit);
        REQUIRE(has(hit, "returns to druid form (the beast form drops to 0 HP)"));
        REQUIRE_FALSE(encounter.combatants()[0].druid_form.has_value());
        REQUIRE(encounter.combatants()[0].current_hp == 19);
        REQUIRE(encounter.character(1)->current_hp == 19);
    }
    SECTION("the beast attacks with its Multiattack, and the druid comes back at the end") {
        ScriptedRandomSource rolls({20, 1, 15});
        dnd::Dice dice(rolls);
        auto encounter = p.fight(dice);
        setup(encounter, 4, "Zombie");
        REQUIRE(encounter.wild_shape("Black Bear"));
        encounter.take_events();
        encounter.next_turn();  // the zombie, then the druid-bear again
        encounter.take_events();
        const auto& bear = encounter.combatants()[0];
        encounter.attack(1, bear.attack_bonus, bear.damage_dice_count, bear.damage_dice_sides, bear.damage_bonus);
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "(Black Bear) uses Multiattack"));
        REQUIRE(has(text, "Druid attacks Zombie"));
        REQUIRE(encounter.use_bonus_action(dnd::Encounter::BonusAction::RevertWildShape));
        REQUIRE(has(text_of(encounter.take_events()), "returns to druid form (a bonus action)"));
        REQUIRE_FALSE(encounter.combatants()[0].druid_form.has_value());
        REQUIRE(encounter.combatants()[0].max_hp == 20);
    }
}
