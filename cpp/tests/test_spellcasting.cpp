#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "dnd/progression.hpp"
#include "dnd/repository.hpp"
#include "dnd/spellcasting.hpp"

namespace {

using dnd::Ability;

class SequenceRandomSource : public dnd::IRandomSource {
public:
    explicit SequenceRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int /*sides*/) override { return values_[index_++ % values_.size()]; }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

const dnd::ClassInfo& cls(const char* name) {
    return *dnd::find_class_info(name);
}

dnd::Spell spell_named(const std::vector<dnd::Spell>& spells, const std::string& name) {
    for (const auto& spell : spells) {
        if (spell.name == name) return spell;
    }
    FAIL("no seeded spell named " << name);
    return {};
}

dnd::CasterStats stats(int level, int attack_bonus, int dc, int modifier) {
    dnd::CasterStats caster;
    caster.level = level;
    caster.spell_attack_bonus = attack_bonus;
    caster.spell_save_dc = dc;
    caster.spellcasting_modifier = modifier;
    return caster;
}

}  // namespace

TEST_CASE("class spellcasting data", "[spellcasting]") {
    REQUIRE(dnd::is_spellcaster(cls("Wizard")));
    REQUIRE_FALSE(dnd::is_spellcaster(cls("Fighter")));
    REQUIRE(cls("Wizard").spellcasting.ability == Ability::Intelligence);
    REQUIRE(cls("Cleric").spellcasting.ability == Ability::Wisdom);
    REQUIRE(cls("Warlock").spellcasting.ability == Ability::Charisma);
    REQUIRE(cls("Ranger").spellcasting.caster_type == dnd::CasterType::Half);
    REQUIRE(cls("Warlock").spellcasting.caster_type == dnd::CasterType::Pact);
}

TEST_CASE("spell save DC and attack bonus", "[spellcasting]") {
    dnd::Character wizard;
    wizard.level = 5;          // proficiency +3
    wizard.intelligence = 16;  // +3
    REQUIRE(dnd::spell_attack_bonus(wizard, cls("Wizard")) == 6);
    REQUIRE(dnd::spell_save_dc(wizard, cls("Wizard")) == 14);

    auto caster = dnd::caster_stats(wizard, cls("Wizard"));
    REQUIRE(caster.level == 5);
    REQUIRE(caster.spellcasting_modifier == 3);
    REQUIRE(caster.spell_save_dc == 14);
}

TEST_CASE("cantrips and spells known follow the PHB class tables", "[spellcasting]") {
    REQUIRE(dnd::cantrips_known(cls("Bard"), 1) == 2);
    REQUIRE(dnd::cantrips_known(cls("Cleric"), 1) == 3);
    REQUIRE(dnd::cantrips_known(cls("Sorcerer"), 1) == 4);
    REQUIRE(dnd::cantrips_known(cls("Wizard"), 4) == 4);
    REQUIRE(dnd::cantrips_known(cls("Wizard"), 10) == 5);
    REQUIRE(dnd::cantrips_known(cls("Paladin"), 10) == 0);
    REQUIRE(dnd::cantrips_known(cls("Fighter"), 1) == 0);

    REQUIRE(dnd::spells_known(cls("Bard"), 1) == 4);
    REQUIRE(dnd::spells_known(cls("Bard"), 10) == 14);
    REQUIRE(dnd::spells_known(cls("Sorcerer"), 1) == 2);
    REQUIRE(dnd::spells_known(cls("Warlock"), 20) == 15);
    REQUIRE(dnd::spells_known(cls("Ranger"), 1) == 0);
    REQUIRE(dnd::spells_known(cls("Ranger"), 2) == 2);
    REQUIRE(dnd::spells_known(cls("Cleric"), 5) == 0);  // prepares instead

    REQUIRE(dnd::spellbook_size(1) == 6);
    REQUIRE(dnd::spellbook_size(3) == 10);
    REQUIRE_THROWS_AS(dnd::spellbook_size(0), std::invalid_argument);
}

TEST_CASE("prepared spell counts", "[spellcasting]") {
    dnd::Character character;
    character.level = 1;
    character.wisdom = 16;        // +3
    character.intelligence = 8;   // -1
    character.charisma = 14;      // +2

    REQUIRE(dnd::prepared_spell_count(character, cls("Cleric")) == 4);
    REQUIRE(dnd::prepared_spell_count(character, cls("Wizard")) == 1);   // -1 + 1, minimum 1
    REQUIRE(dnd::prepared_spell_count(character, cls("Paladin")) == 0);  // no spells at level 1
    REQUIRE(dnd::prepared_spell_count(character, cls("Bard")) == 0);     // a Known caster

    character.level = 5;
    REQUIRE(dnd::prepared_spell_count(character, cls("Paladin")) == 4);  // +2 + half of 5
}

TEST_CASE("spell slot tables", "[spellcasting]") {
    using Slots = std::array<int, 9>;
    REQUIRE(dnd::spell_slots(cls("Wizard"), 1) == Slots{2, 0, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Wizard"), 5) == Slots{4, 3, 2, 0, 0, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Cleric"), 20) == Slots{4, 3, 3, 3, 3, 2, 2, 1, 1});

    REQUIRE(dnd::spell_slots(cls("Paladin"), 1) == Slots{});
    REQUIRE(dnd::spell_slots(cls("Paladin"), 2) == Slots{2, 0, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Ranger"), 5) == Slots{4, 2, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Ranger"), 20) == Slots{4, 3, 3, 3, 2, 0, 0, 0, 0});

    REQUIRE(dnd::spell_slots(cls("Warlock"), 1) == Slots{1, 0, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Warlock"), 3) == Slots{0, 2, 0, 0, 0, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Warlock"), 11) == Slots{0, 0, 0, 0, 3, 0, 0, 0, 0});
    REQUIRE(dnd::spell_slots(cls("Warlock"), 17) == Slots{0, 0, 0, 0, 4, 0, 0, 0, 0});

    REQUIRE(dnd::spell_slots(cls("Fighter"), 20) == Slots{});
}

TEST_CASE("spending slots and resting", "[spellcasting]") {
    dnd::Character wizard;
    wizard.level = 3;  // 4 x 1st, 2 x 2nd
    REQUIRE(dnd::slots_remaining(wizard, cls("Wizard"), 2) == 2);
    REQUIRE(dnd::spend_spell_slot(wizard, cls("Wizard"), 2));
    REQUIRE(dnd::spend_spell_slot(wizard, cls("Wizard"), 2));
    REQUIRE_FALSE(dnd::spend_spell_slot(wizard, cls("Wizard"), 2));
    REQUIRE(dnd::slots_remaining(wizard, cls("Wizard"), 2) == 0);
    REQUIRE_FALSE(dnd::spend_spell_slot(wizard, cls("Wizard"), 3));  // has no 3rd-level slots
    REQUIRE_THROWS_AS(dnd::slots_remaining(wizard, cls("Wizard"), 10), std::invalid_argument);

    dnd::take_short_rest(wizard, cls("Wizard"));  // not a Warlock: nothing back
    REQUIRE(dnd::slots_remaining(wizard, cls("Wizard"), 2) == 0);
    dnd::take_long_rest(wizard);
    REQUIRE(dnd::slots_remaining(wizard, cls("Wizard"), 2) == 2);

    dnd::Character warlock;
    warlock.level = 2;
    REQUIRE(dnd::spend_spell_slot(warlock, cls("Warlock"), 1));
    REQUIRE(dnd::spend_spell_slot(warlock, cls("Warlock"), 1));
    REQUIRE(dnd::slots_remaining(warlock, cls("Warlock"), 1) == 0);
    dnd::take_short_rest(warlock, cls("Warlock"));
    REQUIRE(dnd::slots_remaining(warlock, cls("Warlock"), 1) == 2);
}

TEST_CASE("seeded spells load with their class lists", "[spellcasting][repository]") {
    dnd::Repository repo(":memory:");
    auto spells = repo.get_spells();
    REQUIRE(spells.size() == 42);  // 18 with automated effects + 24 utility
    REQUIRE(spells.front().level == 0);  // ordered by level

    for (const auto& spell : spells) {
        INFO(spell.name);
        REQUIRE_FALSE(spell.class_names.empty());
        for (const auto& class_name : spell.class_names) REQUIRE(dnd::find_class_info(class_name) != nullptr);
        if (spell.resolution == dnd::SpellResolution::Utility) continue;
        REQUIRE(spell.dice_count >= 1);
        REQUIRE_FALSE(spell.concentration);  // automated spells avoid concentration until phase 3
    }

    auto light = spell_named(spells, "Light");
    REQUIRE(light.resolution == dnd::SpellResolution::Utility);
    REQUIRE(spell_named(spells, "Bless").concentration);

    auto missile = spell_named(spells, "Magic Missile");
    REQUIRE(missile.resolution == dnd::SpellResolution::AutoHit);
    REQUIRE(missile.projectiles == 3);
    REQUIRE(missile.flat_bonus == 1);
    REQUIRE(missile.class_names == std::vector<std::string>{"Sorcerer", "Wizard"});

    auto flame = spell_named(spells, "Sacred Flame");
    REQUIRE(flame.resolution == dnd::SpellResolution::Save);
    REQUIRE(flame.save_ability == Ability::Dexterity);
    REQUIRE_FALSE(flame.save_halves);

    auto healing = spell_named(spells, "Healing Word");
    REQUIRE(healing.casting_time == dnd::CastingTime::BonusAction);
    REQUIRE(healing.adds_modifier);

    REQUIRE(spell_named(spells, "Eldritch Blast").cantrip_scaling == dnd::CantripScaling::Projectiles);
    REQUIRE(spell_named(spells, "Inflict Wounds").melee_attack);
}

TEST_CASE("every caster can fill its level-1 picks from the seeded spells", "[spellcasting][repository]") {
    dnd::Repository repo(":memory:");
    auto spells = repo.get_spells();
    for (const auto& info : dnd::class_infos()) {
        if (!dnd::is_spellcaster(info)) continue;
        INFO(info.name);
        int cantrips = 0;
        int first_level = 0;
        for (const auto& spell : spells) {
            if (std::find(spell.class_names.begin(), spell.class_names.end(), info.name) == spell.class_names.end()) {
                continue;
            }
            (spell.level == 0 ? cantrips : first_level) += 1;
        }
        REQUIRE(cantrips >= dnd::cantrips_known(info, 1));

        // The most a level-1 (or, for half casters, level-2) character of
        // this class could need to pick, with a 20 in its casting ability.
        int level = info.spellcasting.caster_type == dnd::CasterType::Half ? 2 : 1;
        dnd::Character best;
        best.level = level;
        best.strength = best.dexterity = best.constitution = best.intelligence = best.wisdom = best.charisma = 20;
        int needed = 0;
        switch (info.spellcasting.preparation) {
            case dnd::SpellPreparation::Known: needed = dnd::spells_known(info, level); break;
            case dnd::SpellPreparation::Prepared: needed = dnd::prepared_spell_count(best, info); break;
            case dnd::SpellPreparation::Spellbook: needed = dnd::spellbook_size(level); break;
            case dnd::SpellPreparation::None: break;
        }
        REQUIRE(first_level >= needed);
    }
}

TEST_CASE("utility spells cast with no targets and no effects", "[spellcasting]") {
    dnd::Repository repo(":memory:");
    SequenceRandomSource rolls({10});
    dnd::Dice dice(rolls);
    auto result = dnd::cast_spell(dice, spell_named(repo.get_spells(), "Detect Magic"), stats(1, 5, 13, 3), 1, {});
    REQUIRE(result.effects.empty());
    REQUIRE(result.slot_level == 1);
}

TEST_CASE("known spells are stored per character", "[spellcasting][repository]") {
    dnd::Repository repo(":memory:");
    int player_id = repo.add_player("Aria");
    dnd::Character character;
    character.character_name = "Aria";
    character.class_id = 1;
    character.race = 1;
    character.alignment = 1;
    character.player_id = player_id;
    character.spell_slots_used = {1, 0, 2, 0, 0, 0, 0, 0, 3};
    int id = repo.add_character(character);

    auto loaded = repo.get_characters()[0];
    REQUIRE(loaded.spell_slots_used == character.spell_slots_used);
    loaded.spell_slots_used.fill(0);
    repo.update_character(loaded);
    REQUIRE(repo.get_characters()[0].spell_slots_used == std::array<int, 9>{});

    auto spells = repo.get_spells();
    int fire_bolt = spell_named(spells, "Fire Bolt").spell_id;
    int missile = spell_named(spells, "Magic Missile").spell_id;
    repo.add_character_spell(id, missile);
    repo.add_character_spell(id, fire_bolt);
    repo.add_character_spell(id, fire_bolt);  // no-op

    auto known = repo.get_character_spells(id);
    REQUIRE(known.size() == 2);
    REQUIRE(known[0].name == "Fire Bolt");  // cantrips first
    REQUIRE(known[1].name == "Magic Missile");
    REQUIRE_FALSE(known[1].class_names.empty());

    repo.remove_character_spell(id, fire_bolt);
    REQUIRE(repo.get_character_spells(id).size() == 1);

    repo.delete_character(id);
    REQUIRE(repo.get_character_spells(id).empty());
}

TEST_CASE("cantrip and upcast scaling", "[spellcasting]") {
    dnd::Repository repo(":memory:");
    auto spells = repo.get_spells();
    auto fire_bolt = spell_named(spells, "Fire Bolt");
    auto blast = spell_named(spells, "Eldritch Blast");
    auto missile = spell_named(spells, "Magic Missile");
    auto cure = spell_named(spells, "Cure Wounds");

    REQUIRE(dnd::cantrip_tier(4) == 1);
    REQUIRE(dnd::cantrip_tier(5) == 2);
    REQUIRE(dnd::cantrip_tier(17) == 4);

    REQUIRE(dnd::spell_dice_count(fire_bolt, 1, 0) == 1);
    REQUIRE(dnd::spell_dice_count(fire_bolt, 11, 0) == 3);
    REQUIRE(dnd::spell_projectiles(fire_bolt, 11, 0) == 1);

    REQUIRE(dnd::spell_dice_count(blast, 17, 0) == 1);
    REQUIRE(dnd::spell_projectiles(blast, 17, 0) == 4);
    REQUIRE(dnd::spell_max_targets(blast, 17, 0) == 4);

    REQUIRE(dnd::spell_projectiles(missile, 1, 1) == 3);
    REQUIRE(dnd::spell_projectiles(missile, 5, 3) == 5);
    REQUIRE(dnd::spell_max_targets(missile, 5, 3) == 5);

    REQUIRE(dnd::spell_dice_count(cure, 3, 2) == 2);
}

TEST_CASE("cast_spell resolves each kind", "[spellcasting]") {
    dnd::Repository repo(":memory:");
    auto spells = repo.get_spells();
    auto caster = stats(1, 5, 13, 3);

    SECTION("Attack: a hit rolls damage, a natural 20 doubles the dice") {
        SequenceRandomSource rolls({15, 7});  // d20 15 (+5 = 20 vs AC 14), then 1d10 = 7
        dnd::Dice dice(rolls);
        auto result = dnd::cast_spell(dice, spell_named(spells, "Fire Bolt"), caster, 0, {{14, 0}});
        REQUIRE(result.effects.size() == 1);
        REQUIRE(result.effects[0].hit);
        REQUIRE(result.effects[0].amount == 7);

        SequenceRandomSource crit({20, 4, 4});  // natural 20, then 2d10
        dnd::Dice crit_dice(crit);
        result = dnd::cast_spell(crit_dice, spell_named(spells, "Fire Bolt"), caster, 0, {{14, 0}});
        REQUIRE(result.effects[0].critical);
        REQUIRE(result.effects[0].amount == 8);  // 2d10 at 4 each
    }

    SECTION("Save: shared damage, per-target saves, half on a success") {
        // 3d6 = 4+4+4 = 12; then target saves: 2 (+1 = 3, fails), 15 (+1 = 16, saves)
        SequenceRandomSource rolls({4, 4, 4, 2, 15});
        dnd::Dice dice(rolls);
        auto result = dnd::cast_spell(dice, spell_named(spells, "Burning Hands"), caster, 1, {{12, 1}, {12, 1}});
        REQUIRE(result.dice == std::vector<int>{4, 4, 4});
        REQUIRE(result.effects.size() == 2);
        REQUIRE_FALSE(result.effects[0].saved);
        REQUIRE(result.effects[0].amount == 12);
        REQUIRE(result.effects[1].saved);
        REQUIRE(result.effects[1].amount == 6);
    }

    SECTION("Save without halving deals nothing on a success") {
        SequenceRandomSource rolls({8, 18});  // 1d8 = 8, save 18 +0 >= 13
        dnd::Dice dice(rolls);
        auto result = dnd::cast_spell(dice, spell_named(spells, "Sacred Flame"), caster, 0, {{12, 0}});
        REQUIRE(result.effects[0].saved);
        REQUIRE(result.effects[0].amount == 0);
    }

    SECTION("AutoHit: darts share targets round-robin; upcasting adds darts") {
        SequenceRandomSource rolls({2});  // every d4 = 2, so each dart = 3
        dnd::Dice dice(rolls);
        auto result = dnd::cast_spell(dice, spell_named(spells, "Magic Missile"), caster, 2, {{10, 0}, {10, 0}});
        REQUIRE(result.projectiles == 4);
        REQUIRE(result.effects.size() == 4);
        std::vector<int> targets;
        for (const auto& effect : result.effects) {
            REQUIRE(effect.hit);
            REQUIRE(effect.amount == 3);
            targets.push_back(effect.target);
        }
        REQUIRE(targets == std::vector<int>{0, 1, 0, 1});
    }

    SECTION("Heal adds the spellcasting modifier") {
        SequenceRandomSource rolls({5});
        dnd::Dice dice(rolls);
        auto result = dnd::cast_spell(dice, spell_named(spells, "Cure Wounds"), caster, 1, {{}});
        REQUIRE(result.effects[0].amount == 8);  // 5 + 3
    }

    SECTION("invalid slot levels and target counts throw") {
        SequenceRandomSource rolls({10});
        dnd::Dice dice(rolls);
        auto fire_bolt = spell_named(spells, "Fire Bolt");
        auto burning = spell_named(spells, "Burning Hands");
        REQUIRE_THROWS_AS(dnd::cast_spell(dice, fire_bolt, caster, 1, {{}}), std::invalid_argument);
        REQUIRE_THROWS_AS(dnd::cast_spell(dice, burning, caster, 0, {{}}), std::invalid_argument);
        REQUIRE_THROWS_AS(dnd::cast_spell(dice, burning, caster, 10, {{}}), std::invalid_argument);
        REQUIRE_THROWS_AS(dnd::cast_spell(dice, burning, caster, 1, {}), std::invalid_argument);
        REQUIRE_THROWS_AS(dnd::cast_spell(dice, burning, caster, 1, {{}, {}, {}, {}}), std::invalid_argument);
    }
}

TEST_CASE("spell_allowance and validate_spell_selection", "[spellcasting]") {
    dnd::Repository repo(":memory:");
    auto spells = repo.get_spells();

    dnd::Character wizard;
    wizard.level = 1;
    wizard.intelligence = 16;  // +3
    auto allowance = dnd::spell_allowance(wizard, cls("Wizard"));
    REQUIRE(allowance.cantrips == 3);
    REQUIRE(allowance.spells == 4);  // prepared: +3 + level 1
    REQUIRE(allowance.max_spell_level == 1);

    dnd::Character bard;
    bard.level = 1;
    REQUIRE(dnd::spell_allowance(bard, cls("Bard")).spells == 4);  // known

    dnd::Character paladin;
    paladin.level = 1;
    paladin.charisma = 16;
    allowance = dnd::spell_allowance(paladin, cls("Paladin"));
    REQUIRE(allowance.cantrips == 0);
    REQUIRE(allowance.spells == 0);
    REQUIRE(allowance.max_spell_level == 0);
    paladin.level = 2;
    REQUIRE(dnd::spell_allowance(paladin, cls("Paladin")).spells == 4);  // +3 + 1

    dnd::Character warlock;
    warlock.level = 3;
    REQUIRE(dnd::spell_allowance(warlock, cls("Warlock")).max_spell_level == 2);  // pact slots are 2nd level
    REQUIRE(dnd::spell_allowance(warlock, cls("Fighter")).max_spell_level == 0);

    std::vector<dnd::Spell> picks = {spell_named(spells, "Fire Bolt"), spell_named(spells, "Light"),
                                     spell_named(spells, "Mage Hand"), spell_named(spells, "Magic Missile"),
                                     spell_named(spells, "Mage Armor"), spell_named(spells, "Detect Magic"),
                                     spell_named(spells, "Burning Hands")};
    REQUIRE_FALSE(dnd::validate_spell_selection(wizard, cls("Wizard"), picks, true));

    auto fewer = picks;
    fewer.pop_back();
    REQUIRE(dnd::validate_spell_selection(wizard, cls("Wizard"), fewer, true));         // must be full at creation
    REQUIRE_FALSE(dnd::validate_spell_selection(wizard, cls("Wizard"), fewer, false));  // fine when learning

    auto wrong_list = picks;
    wrong_list.back() = spell_named(spells, "Cure Wounds");
    REQUIRE(dnd::validate_spell_selection(wizard, cls("Wizard"), wrong_list, true));

    auto duplicate = picks;
    duplicate.back() = picks.front();
    REQUIRE(dnd::validate_spell_selection(wizard, cls("Wizard"), duplicate, false));

    auto too_many = picks;
    too_many.push_back(spell_named(spells, "Thunderwave"));
    REQUIRE(dnd::validate_spell_selection(wizard, cls("Wizard"), too_many, false));
}

TEST_CASE("ordinal", "[spellcasting]") {
    REQUIRE(dnd::ordinal(1) == "1st");
    REQUIRE(dnd::ordinal(2) == "2nd");
    REQUIRE(dnd::ordinal(3) == "3rd");
    REQUIRE(dnd::ordinal(4) == "4th");
    REQUIRE(dnd::ordinal(9) == "9th");
    REQUIRE(dnd::ordinal(11) == "11th");
    REQUIRE(dnd::ordinal(21) == "21st");
}
