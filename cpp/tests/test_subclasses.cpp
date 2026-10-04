#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/class_features.hpp"
#include "dnd/encounter.hpp"
#include "dnd/equipment.hpp"
#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"
#include "dnd/spellbook.hpp"
#include "dnd/spellcasting.hpp"
#include "dnd/subclass.hpp"

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

struct Table {
    dnd::Repository repo{":memory:"};

    int class_id(const std::string& name) {
        for (const auto& c : repo.get_classes()) {
            if (c.name == name) return c.id;
        }
        return 0;
    }

    // A level-N character of `class_name` with 16s in every casting stat, STR 16, DEX 14.
    dnd::Character hero(const std::string& class_name, int level, const std::string& subclass = "",
                        const std::string& choice = "", int id = 1) {
        dnd::Character c;
        c.character_id = id;
        c.character_name = class_name + std::to_string(id);
        c.class_id = class_id(class_name);
        c.level = level;
        c.strength = 16;
        c.dexterity = 14;
        c.constitution = 14;
        c.intelligence = 16;
        c.wisdom = 16;
        c.charisma = 16;
        c.max_hp = 40;
        c.current_hp = 40;
        c.subclass = subclass;
        c.subclass_choice = choice;
        return c;
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

    dnd::Spell spell(const std::string& name) {
        for (const auto& s : repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return {};
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

TEST_CASE("the twelve SRD subclasses", "[subclasses]") {
    REQUIRE(dnd::subclasses().size() == 12);
    for (const char* class_name : {"Barbarian", "Bard", "Cleric", "Druid", "Fighter", "Monk", "Paladin", "Ranger",
                                   "Rogue", "Sorcerer", "Warlock", "Wizard"}) {
        INFO(class_name);
        REQUIRE(dnd::subclasses_for(class_name).size() == 1);
        REQUIRE(dnd::find_class_info(class_name) != nullptr);
    }
    REQUIRE(dnd::subclass_level("Cleric") == 1);
    REQUIRE(dnd::subclass_level("Wizard") == 2);
    REQUIRE(dnd::subclass_level("Fighter") == 3);
    REQUIRE(dnd::subclass_kind("Warlock") == "Otherworldly Patron");

    SECTION("every subclass spell is seeded") {
        Table t;
        auto spells = t.repo.get_spells();
        for (const auto& info : dnd::subclasses()) {
            for (const auto& s : info.spells) {
                INFO(info.name << ": " << s.spell);
                REQUIRE(std::any_of(spells.begin(), spells.end(), [&](const dnd::Spell& x) { return x.name == s.spell; }));
            }
        }
    }
}

TEST_CASE("choosing a subclass", "[subclasses]") {
    Table t;
    auto fighter = t.hero("Fighter", 2);
    REQUIRE_FALSE(dnd::subclass_due(fighter, "Fighter"));
    REQUIRE(dnd::subclass_refusal(fighter, "Fighter", "Champion", "") ==
            "Fighters choose a Martial Archetype at level 3.");
    fighter.level = 3;
    REQUIRE(dnd::subclass_due(fighter, "Fighter"));
    REQUIRE(dnd::subclass_refusal(fighter, "Fighter", "Thief", "") == "Thief isn't a Fighter subclass.");
    dnd::choose_subclass(fighter, "Fighter", "Champion", "");
    REQUIRE(fighter.subclass == "Champion");
    REQUIRE_FALSE(dnd::subclass_due(fighter, "Fighter"));

    SECTION("a choice is required when the subclass has one") {
        auto druid = t.hero("Druid", 2);
        REQUIRE(dnd::subclass_refusal(druid, "Druid", "Circle of the Land", "") == "Choose a Land.");
        dnd::choose_subclass(druid, "Druid", "Circle of the Land", "Forest");
        REQUIRE(druid.subclass_choice == "Forest");
    }
    SECTION("Draconic Resilience adds a hit point per level, now and on level up") {
        auto sorcerer = t.hero("Sorcerer", 3);
        dnd::choose_subclass(sorcerer, "Sorcerer", "Draconic Bloodline", "Red (fire)");
        REQUIRE(sorcerer.max_hp == 43);
        sorcerer.experience = dnd::xp_threshold_for_level(4);
        auto plan = dnd::level_up_plan(sorcerer, *dnd::find_class_info("Sorcerer"));
        dnd::LevelUpChoices choices;
        choices.ability_increases = {dnd::Ability::Charisma, dnd::Ability::Charisma};
        int before = sorcerer.max_hp;
        dnd::apply_level_up(sorcerer, plan, choices);
        REQUIRE(sorcerer.max_hp == before + 4 + 2 + 1);  // average d6 (4) + CON +2 + Draconic +1
    }
}

TEST_CASE("subclass spells and features outside combat", "[subclasses]") {
    Table t;

    SECTION("a Life cleric's domain spells are always prepared and free") {
        auto cleric = t.hero("Cleric", 3, "Life Domain");
        cleric.player_id = t.repo.add_player("Adam");
        cleric.race = t.repo.get_races().front().id;
        cleric.alignment = t.repo.get_alignments().front().id;
        cleric.character_id = t.repo.add_character(cleric);
        auto ready = dnd::spells_ready(t.repo, cleric);
        std::vector<std::string> names;
        for (const auto& s : ready) names.push_back(s.name);
        for (const char* name : {"Bless", "Cure Wounds", "Lesser Restoration", "Spiritual Weapon"}) {
            REQUIRE(std::find(names.begin(), names.end(), name) != names.end());
        }
        REQUIRE(std::find(names.begin(), names.end(), "Revivify") == names.end());  // level 5
    }
    SECTION("a Land druid's circle spells follow the land") {
        auto druid = t.hero("Druid", 5, "Circle of the Land", "Arctic");
        auto names = dnd::always_prepared_spell_names(druid);
        REQUIRE(names == std::vector<std::string>{"Hold Person", "Spike Growth", "Sleet Storm", "Slow"});
        REQUIRE(dnd::spell_allowance(druid, *dnd::find_class_info("Druid")).cantrips ==
                dnd::cantrips_known(*dnd::find_class_info("Druid"), 5) + 1);  // Bonus Cantrip
    }
    SECTION("a Fiend warlock can learn Fireball") {
        auto warlock = t.hero("Warlock", 5, "The Fiend");
        auto fireball = t.spell("Fireball");
        REQUIRE(dnd::on_spell_list(fireball, "Warlock", warlock));
        REQUIRE_FALSE(dnd::on_spell_list(fireball, "Warlock", t.hero("Warlock", 5)));
        REQUIRE_FALSE(dnd::validate_spell_selection(warlock, *dnd::find_class_info("Warlock"), {fireball}, false));
        REQUIRE(dnd::always_prepared_spell_names(warlock).empty());  // expanded, not free
    }
    SECTION("Draconic Resilience AC, and Life Domain's heavy armor") {
        auto sorcerer = t.hero("Sorcerer", 1, "Draconic Bloodline", "Gold (fire)");
        REQUIRE(dnd::calculate_armor_class(sorcerer, "Sorcerer", {}).total == 15);  // 13 + DEX 2
        REQUIRE(dnd::is_proficient_with_armor("Cleric", t.item("Plate Armor"), "Life Domain"));
        REQUIRE_FALSE(dnd::is_proficient_with_armor("Cleric", t.item("Plate Armor")));
    }
    SECTION("Channel Divinity uses") {
        REQUIRE(dnd::feature_max_uses(dnd::Feature::ChannelDivinity, t.hero("Cleric", 1), "Cleric") == 0);
        REQUIRE(dnd::feature_max_uses(dnd::Feature::ChannelDivinity, t.hero("Cleric", 2), "Cleric") == 1);
        REQUIRE(dnd::feature_max_uses(dnd::Feature::ChannelDivinity, t.hero("Cleric", 6), "Cleric") == 2);
        REQUIRE(dnd::feature_max_uses(dnd::Feature::ChannelDivinity, t.hero("Paladin", 3), "Paladin") == 1);
        REQUIRE(dnd::feature_recovery(dnd::Feature::ChannelDivinity, 2) == dnd::Recovery::ShortRest);
    }
    SECTION("Natural Recovery on a short rest") {
        auto druid = t.hero("Druid", 4, "Circle of the Land", "Coast");
        druid.spell_slots_used = {2, 1, 0, 0, 0, 0, 0, 0, 0};
        auto recovered = dnd::use_arcane_recovery(druid, "Druid");  // budget 2: one 2nd-level slot
        REQUIRE(recovered[1] == 1);
        REQUIRE(druid.spell_slots_used[1] == 0);
        REQUIRE(dnd::use_arcane_recovery(druid, "Druid")[0] == 0);  // once per long rest
        auto plain = t.hero("Druid", 4);
        plain.spell_slots_used = {2, 0, 0, 0, 0, 0, 0, 0, 0};
        REQUIRE(dnd::use_arcane_recovery(plain, "Druid")[0] == 0);
    }
    SECTION("Level Up names the subclass's features instead of the placeholder") {
        auto fighter = t.hero("Fighter", 6, "Champion");
        fighter.experience = dnd::xp_threshold_for_level(7);
        auto plan = dnd::level_up_plan(fighter, *dnd::find_class_info("Fighter"));
        std::vector<std::string> names;
        for (const auto& f : plan.features) names.push_back(f.name);
        REQUIRE(std::find(names.begin(), names.end(), "Remarkable Athlete") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "Martial Archetype feature") == names.end());
    }
    SECTION("a long rest removes a level of exhaustion and temporary HP") {
        auto fighter = t.hero("Fighter", 3);
        fighter.exhaustion = 2;
        fighter.temp_hp = 7;
        dnd::take_long_rest(fighter);
        REQUIRE(fighter.exhaustion == 1);
        REQUIRE(fighter.temp_hp == 0);
    }
}

TEST_CASE("subclass features in combat", "[subclasses][encounter]") {
    Table t;
    auto fight = [&](dnd::Dice& dice) { return dnd::Encounter(dice, t.repo.get_classes(), t.repo.get_races()); };
    std::vector<dnd::InventoryEntry> longsword = {{t.item("Longsword"), 1, true}};

    SECTION("Champion: a natural 19 crits") {
        ScriptedRandomSource rolls({20, 1, 19, 5, 5});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Fighter", 3, "Champion"), {}, longsword);
        encounter.add_monsters(t.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        const auto& me = encounter.combatants()[0];
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        REQUIRE(has(text_of(encounter.take_events()), "CRITICAL HIT"));
    }
    SECTION("Hunter: Colossus Slayer against the wounded, once per turn") {
        // Two attacks at level 5: the first hits a fresh zombie, the second a hurt one (+1d8).
        ScriptedRandomSource rolls({20, 1, 15, 3, 15, 3, 4});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Ranger", 5, "Hunter"), {}, longsword);
        encounter.add_monsters(t.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        const auto& me = encounter.combatants()[0];
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        REQUIRE_FALSE(has(text_of(encounter.take_events()), "Colossus Slayer"));
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        REQUIRE(has(text_of(encounter.take_events()), "Colossus Slayer"));
    }
    SECTION("Fiend: Dark One's Blessing on a kill, and temporary HP soak damage") {
        // Initiative: the warlock 20, the goblin 1. Fire Bolt: d20 18 hits, 1d10 = 10 kills the goblin.
        ScriptedRandomSource rolls({20, 1, 18, 10});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Warlock", 3, "The Fiend"), {t.spell("Fire Bolt")}, {});
        encounter.add_monsters(t.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.cast_spell(t.spell("Fire Bolt").spell_id, 0, {1}));
        REQUIRE(has(text_of(encounter.take_events()), "Dark One's Blessing: Warlock1 gains 6 temporary hit points"));
        REQUIRE(encounter.combatants()[0].temp_hp == 6);  // CHA +3 + level 3
        REQUIRE(encounter.character(1)->temp_hp == 6);
    }
    SECTION("exhaustion 3: disadvantage on attack rolls") {
        auto tired = t.hero("Fighter", 3);
        tired.exhaustion = 3;
        ScriptedRandomSource rolls({20, 20, 1, 15, 3});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(tired, {}, longsword);
        encounter.add_monsters(t.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        const auto& me = encounter.combatants()[0];
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        REQUIRE(has(text_of(encounter.take_events()), "with disadvantage (exhaustion)"));
    }
    SECTION("exhaustion 4 halves max HP in a fight") {
        auto worn = t.hero("Fighter", 3);
        worn.exhaustion = 4;
        ScriptedRandomSource rolls({20});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(worn, {}, longsword);
        REQUIRE(encounter.combatants()[0].max_hp == 20);
        REQUIRE(encounter.combatants()[0].current_hp == 20);
    }
    SECTION("Berserker: Frenzy attacks from the next turn, exhaustion after") {
        ScriptedRandomSource rolls({20, 1, 1});  // the barbarian first; everything else misses
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Barbarian", 3, "Path of the Berserker"), {}, longsword);
        encounter.add_monsters(t.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.use_bonus_action(dnd::Encounter::BonusAction::RageWithFrenzy));
        const auto& me = encounter.combatants()[0];
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);  // keeps the rage up
        encounter.next_turn();  // the zombie, then the barbarian again
        auto options = encounter.bonus_action_options();
        REQUIRE(std::any_of(options.begin(), options.end(), [](const auto& o) {
            return o.kind == dnd::Encounter::BonusAction::FrenzyAttack;
        }));
        REQUIRE(encounter.use_bonus_action(dnd::Encounter::BonusAction::FrenzyAttack, 1));
        REQUIRE(has(text_of(encounter.take_events()), "attacks in a frenzy"));
        REQUIRE(encounter.character(1)->exhaustion == 0);  // not until the rage ends
    }
    SECTION("Lore bard: Cutting Words turns a hit into a miss") {
        // Initiative: the bard 1, the goblin 20. Scimitar d20 12 + 4 = 16 vs AC 12; Cutting Words d6 = 6 -> 10.
        ScriptedRandomSource rolls({1, 20, 1, 12, 6});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Bard", 3, "College of Lore"), {}, {});
        encounter.add_monsters(t.monster("Goblin"), 1);
        encounter.roll_initiative();
        auto text = text_of(encounter.take_events());
        INFO(text);
        REQUIRE(has(text, "uses Cutting Words (d6: 6)"));
        REQUIRE(has(text, "and it misses"));
        REQUIRE(encounter.combatants()[0].current_hp == 40);
    }
    SECTION("Turn Undead: turned, or destroyed from level 5") {
        // The cleric first; each skeleton's WIS save rolls 1.
        ScriptedRandomSource rolls({20, 1, 1, 1});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Cleric", 2), {}, {});
        encounter.add_monsters(t.monster("Skeleton"), 1);
        encounter.add_monsters(t.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.channel_divinity(dnd::Encounter::Channel::TurnUndead));
        auto text = text_of(encounter.take_events());
        REQUIRE(has(text, "Skeleton: WIS save"));
        REQUIRE(has(text, "-> turned."));
        REQUIRE_FALSE(has(text, "Goblin:"));  // not undead
        REQUIRE(dnd::has_effect(encounter.effects(), 1, dnd::EffectKind::Turned));

        SECTION("a level-5 cleric destroys CR 1/2 undead") {
            ScriptedRandomSource rolls5({20, 1, 1});
            dnd::Dice dice5(rolls5);
            auto five = fight(dice5);
            five.add_character(t.hero("Cleric", 5), {}, {});
            five.add_monsters(t.monster("Skeleton"), 1);
            five.roll_initiative();
            five.take_events();
            REQUIRE(five.channel_divinity(dnd::Encounter::Channel::TurnUndead));
            REQUIRE(has(text_of(five.take_events()), "-> destroyed!"));
            REQUIRE(five.status() == dnd::EncounterStatus::Victory);
        }
    }
    SECTION("Life Domain: Preserve Life, up to half max each") {
        ScriptedRandomSource rolls({20, 19, 1});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        auto cleric = t.hero("Cleric", 2, "Life Domain");
        auto hurt = t.hero("Fighter", 2, "", "", 2);
        hurt.current_hp = 5;  // half max is 20
        encounter.add_character(cleric, {}, {});
        encounter.add_character(hurt, {}, {});
        encounter.add_monsters(t.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.channel_divinity(dnd::Encounter::Channel::PreserveLife));
        REQUIRE(encounter.combatants()[1].current_hp == 15);  // the pool of 10, all to the fighter
    }
    SECTION("Life Domain: Disciple of Life adds 2 + the spell's level") {
        ScriptedRandomSource rolls({20, 19, 1, 4});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        auto hurt = t.hero("Fighter", 2, "", "", 2);
        hurt.current_hp = 5;
        encounter.add_character(t.hero("Cleric", 1, "Life Domain"), {t.spell("Cure Wounds")}, {});
        encounter.add_character(hurt, {}, {});
        encounter.add_monsters(t.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.cast_spell(t.spell("Cure Wounds").spell_id, 1, {1}));
        REQUIRE(encounter.combatants()[1].current_hp == 5 + 4 + 3 + 3);  // d8 4 + WIS 3 + (2 + 1)
    }
    SECTION("Devotion: Sacred Weapon adds CHA to attack rolls") {
        ScriptedRandomSource rolls({20, 1, 10, 3});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        encounter.add_character(t.hero("Paladin", 3, "Oath of Devotion"), {}, longsword);
        encounter.add_monsters(t.monster("Zombie"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.channel_divinity(dnd::Encounter::Channel::SacredWeapon));
        encounter.next_turn();
        encounter.take_events();
        const auto& me = encounter.combatants()[0];
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        REQUIRE(has(text_of(encounter.take_events()), "+3 (Sacred Weapon)"));
    }
    SECTION("Thief: Fast Hands drinks a potion as a bonus action") {
        ScriptedRandomSource rolls({20, 1, 2});
        dnd::Dice dice(rolls);
        auto encounter = fight(dice);
        auto thief = t.hero("Rogue", 3, "Thief");
        thief.current_hp = 10;
        auto potion = t.item("Potion of Healing");
        encounter.add_character(thief, {}, {{potion, 1}});
        encounter.add_monsters(t.monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        REQUIRE(encounter.use_bonus_action(dnd::Encounter::BonusAction::FastHandsPotion, potion.item_id));
        REQUIRE(encounter.combatants()[0].current_hp > 10);
        REQUIRE(encounter.awaiting_action());  // the action is still free
    }
}
