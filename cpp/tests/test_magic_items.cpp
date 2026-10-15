#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/character_io.hpp"
#include "dnd/encounter.hpp"
#include "dnd/equipment.hpp"
#include "dnd/inventory.hpp"
#include "dnd/magic_items.hpp"
#include "dnd/repository.hpp"
#include "dnd/shop.hpp"

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

struct Magic {
    dnd::Repository repo{":memory:"};
    int player_id = repo.add_player("Adam");

    dnd::Item item(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i;
        }
        FAIL("no item " << name);
        return {};
    }

    int class_id(const std::string& name) {
        for (const auto& c : repo.get_classes()) {
            if (c.name == name) return c.id;
        }
        FAIL("no class " << name);
        return 0;
    }

    // STR 16 / DEX 12 / CON 14, 30 HP, level 3.
    int add(const std::string& name, const std::string& class_name = "Fighter") {
        dnd::Character c;
        c.character_name = name;
        c.class_id = class_id(class_name);
        c.race = repo.get_races().front().id;
        c.alignment = repo.get_alignments().front().id;
        c.player_id = player_id;
        c.level = 3;
        c.strength = 16;
        c.dexterity = 12;
        c.constitution = 14;
        c.max_hp = 30;
        c.current_hp = 30;
        return repo.add_character(c);
    }

    dnd::Character get(int id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == id) return c;
        }
        FAIL("no character " << id);
        return {};
    }

    // Carries the item, equipped (weapons and armor) or not.
    void carry(int id, const std::string& name, bool equipped = false) {
        int item_id = item(name).item_id;
        repo.add_to_inventory(id, item_id);
        if (equipped) repo.set_equipped(id, item_id, true);
    }
};

dnd::InventoryEntry entry(const dnd::Item& item, bool equipped = false, bool attuned = false) {
    return {item, 1, equipped, attuned};
}

}  // namespace

TEST_CASE("magic items are seeded", "[magic_items]") {
    Magic m;
    auto items = m.repo.get_items();
    auto count = [&](auto pred) { return std::count_if(items.begin(), items.end(), pred); };

    // +1/+2/+3 for 16 weapons and 13 armors and the shield.
    REQUIRE(count([](const dnd::Item& i) { return i.magic_bonus > 0; }) == 87);
    REQUIRE(count([](const dnd::Item& i) { return i.category == dnd::ItemCategory::Wondrous; }) == 5);
    REQUIRE(count([](const dnd::Item& i) { return i.category == dnd::ItemCategory::Scroll; }) > 20);

    auto sword = m.item("Longsword +2");
    REQUIRE(sword.base_item == "Longsword");
    REQUIRE(sword.rarity == dnd::Rarity::Rare);
    REQUIRE(sword.weapon.has_value());
    REQUIRE(sword.weapon->versatile_dice_sides == 10);  // stats copied from the Longsword
    REQUIRE(sword.cost_cp == m.item("Longsword").cost_cp + 400000);
    REQUIRE_FALSE(sword.requires_attunement);

    auto plate = m.item("Plate Armor +3");
    REQUIRE(plate.rarity == dnd::Rarity::Legendary);
    REQUIRE(plate.armor->base_ac == 18);
    REQUIRE(m.item("Shield +1").rarity == dnd::Rarity::Uncommon);

    auto greater = m.item("Potion of Greater Healing");
    REQUIRE(dnd::is_healing_item(greater));
    REQUIRE(greater.heal_dice_count == 4);
    REQUIRE(greater.heal_bonus == 4);
    REQUIRE(m.item("Potion of Healing").rarity == dnd::Rarity::Common);

    auto cloak = m.item("Cloak of Protection");
    REQUIRE(cloak.requires_attunement);
    REQUIRE(cloak.ac_bonus == 1);
    REQUIRE(cloak.save_bonus == 1);
    REQUIRE(m.item("Amulet of Health").sets_ability == "Constitution");

    auto scroll = m.item("Spell Scroll (Magic Missile)");
    REQUIRE(scroll.scroll_spell_id > 0);
    REQUIRE(scroll.rarity == dnd::Rarity::Common);
    REQUIRE(scroll.cost_cp == 7500);
    REQUIRE(m.item("Spell Scroll (Fire Bolt)").cost_cp == 5000);
}

TEST_CASE("magic weapons and armor", "[magic_items]") {
    Magic m;
    auto aria = m.get(m.add("Aria"));

    SECTION("a +1 weapon adds to attack and damage") {
        auto plain = dnd::weapon_attack(aria, "Fighter", "Human", {entry(m.item("Longsword"), true)});
        auto magic = dnd::weapon_attack(aria, "Fighter", "Human", {entry(m.item("Longsword +1"), true)});
        REQUIRE(magic.name == "Longsword +1");
        REQUIRE(magic.attack_bonus == plain.attack_bonus + 1);
        REQUIRE(magic.damage_bonus == plain.damage_bonus + 1);
    }
    SECTION("proficiency follows the mundane weapon") {
        // Bards are trained with rapiers, not martial weapons in general.
        REQUIRE(dnd::is_proficient_with_weapon("Bard", "Human", m.item("Rapier +2")));
        REQUIRE_FALSE(dnd::is_proficient_with_weapon("Bard", "Human", m.item("Greataxe +2")));
    }
    SECTION("magic armor and shields add to AC") {
        auto ac = dnd::calculate_armor_class(aria, "Fighter",
                                             {entry(m.item("Chain Mail +1"), true), entry(m.item("Shield +2"), true)});
        REQUIRE(ac.total == 17 + 4);
        REQUIRE(ac.text() == "Chain Mail +1 17 + Shield +2 4 = 21");
    }
    SECTION("an unequipped magic weapon does nothing") {
        auto attack = dnd::weapon_attack(aria, "Fighter", "Human", {entry(m.item("Longsword +3"))});
        REQUIRE(attack.name == "Unarmed Strike");
    }
}

TEST_CASE("wondrous items work only while attuned", "[magic_items]") {
    Magic m;
    auto aria = m.get(m.add("Aria"));
    auto cloak = m.item("Cloak of Protection");

    SECTION("Cloak of Protection: +1 AC and saves") {
        REQUIRE(dnd::calculate_armor_class(aria, "Fighter", {entry(cloak)}).total == 11);  // not attuned
        auto ac = dnd::calculate_armor_class(aria, "Fighter", {entry(cloak, false, true)});
        REQUIRE(ac.total == 12);
        REQUIRE(ac.text().find("Cloak of Protection 1") != std::string::npos);
        int plain = dnd::saving_throw_bonus(aria, dnd::Ability::Wisdom);
        REQUIRE(dnd::saving_throw_bonus(aria, dnd::Ability::Wisdom, {entry(cloak, false, true)}) == plain + 1);
    }
    SECTION("Bracers of Defense: only with no armor and no shield") {
        auto bracers = entry(m.item("Bracers of Defense"), false, true);
        REQUIRE(dnd::calculate_armor_class(aria, "Fighter", {bracers}).total == 13);
        REQUIRE(dnd::calculate_armor_class(aria, "Fighter", {bracers, entry(m.item("Shield"), true)}).total == 13);
    }
    SECTION("Gauntlets of Ogre Power: STR 19 for attacks") {
        std::vector<dnd::InventoryEntry> gear = {entry(m.item("Longsword"), true),
                                                 entry(m.item("Gauntlets of Ogre Power"), false, true)};
        auto attack = dnd::weapon_attack(aria, "Fighter", "Human", gear);
        REQUIRE(attack.damage_bonus == 4);  // STR 19, not 16
        REQUIRE(dnd::with_item_scores(aria, gear).strength == 19);
        aria.strength = 20;
        REQUIRE(dnd::with_item_scores(aria, gear).strength == 20);  // never lowers a score
    }
    SECTION("wondrous items aren't equipped; they're attuned") {
        std::vector<dnd::InventoryEntry> pack = {entry(cloak)};
        auto result = dnd::equip(pack, cloak.item_id, aria, "Fighter", "Human");
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error == "Cloak of Protection is worn by attuning to it, which takes a short rest.");
    }
}

TEST_CASE("attunement", "[magic_items]") {
    Magic m;
    int aria = m.add("Aria");
    for (const char* name : {"Cloak of Protection", "Ring of Protection", "Bracers of Defense",
                             "Gauntlets of Ogre Power", "Longsword +1"}) {
        m.carry(aria, name);
    }

    auto attuned = dnd::attune(m.repo, aria, m.item("Cloak of Protection").item_id);
    REQUIRE(attuned.ok);
    REQUIRE(attuned.message == "Aria attunes to the Cloak of Protection.");
    REQUIRE(dnd::attune(m.repo, aria, m.item("Ring of Protection").item_id).ok);
    REQUIRE(dnd::attune(m.repo, aria, m.item("Bracers of Defense").item_id).ok);
    REQUIRE(dnd::attuned_count(m.repo.get_inventory(aria)) == 3);

    auto fourth = dnd::attune(m.repo, aria, m.item("Gauntlets of Ogre Power").item_id);
    REQUIRE_FALSE(fourth.ok);
    REQUIRE(fourth.message.find("Already attuned to 3 items") == 0);
    REQUIRE_FALSE(dnd::attune(m.repo, aria, m.item("Cloak of Protection").item_id).ok);  // already
    REQUIRE(dnd::attune(m.repo, aria, m.item("Longsword +1").item_id).message ==
            "Longsword +1 doesn't need attunement.");

    auto ended = dnd::end_attunement(m.repo, aria, m.item("Ring of Protection").item_id);
    REQUIRE(ended.ok);
    REQUIRE(dnd::attune(m.repo, aria, m.item("Gauntlets of Ogre Power").item_id).ok);

    SECTION("attuned items can't be sold or given away") {
        int phi = m.add("Phi");
        auto sold = dnd::sell(m.repo, aria, m.item("Cloak of Protection").item_id);
        REQUIRE_FALSE(sold.ok);
        REQUIRE(sold.message == "End attunement to the Cloak of Protection (a short rest) before selling it.");
        REQUIRE_FALSE(dnd::give_item(m.repo, aria, phi, m.item("Cloak of Protection").item_id).ok);
        REQUIRE(dnd::give_item(m.repo, aria, phi, m.item("Ring of Protection").item_id).ok);  // no longer attuned
    }
    SECTION("attunement travels in exports") {
        auto file = dnd::export_character(m.repo, aria);
        auto parsed = dnd::parse_character_file(dnd::to_json(file));
        REQUIRE(parsed.file.has_value());
        int attuned_items = 0;
        for (const auto& item : parsed.file->characters.at(0).inventory) attuned_items += item.attuned ? 1 : 0;
        REQUIRE(attuned_items == 3);
    }
}

TEST_CASE("an Amulet of Health's hit points", "[magic_items]") {
    Magic m;
    int aria = m.add("Aria");  // CON 14 (+2), level 3, 30 HP
    m.carry(aria, "Amulet of Health");
    int amulet = m.item("Amulet of Health").item_id;

    auto damaged = m.get(aria);
    damaged.current_hp = 20;
    m.repo.update_character(damaged);

    auto attuned = dnd::attune(m.repo, aria, amulet);
    REQUIRE(attuned.ok);
    REQUIRE(attuned.message == "Aria attunes to the Amulet of Health. Max HP +6.");  // CON 19 (+4): +2 × 3 levels
    REQUIRE(m.get(aria).max_hp == 36);
    REQUIRE(m.get(aria).current_hp == 26);

    SECTION("levelling up keeps the bonus in step") {
        auto c = m.get(aria);
        auto inventory = m.repo.get_inventory(aria);
        dnd::without_item_hp(c, inventory, [](dnd::Character& leveled) {
            REQUIRE(leveled.max_hp == 30);  // the bonus is off while levelling
            leveled.level = 4;
            leveled.max_hp += 8;
            leveled.current_hp += 8;
        });
        REQUIRE(c.max_hp == 38 + 8);  // +2 × 4 levels
    }
    SECTION("ending attunement takes it back") {
        auto ended = dnd::end_attunement(m.repo, aria, amulet);
        REQUIRE(ended.message == "Aria ends attunement to the Amulet of Health. Max HP -6.");
        REQUIRE(m.get(aria).max_hp == 30);
        REQUIRE(m.get(aria).current_hp == 26);
    }
    SECTION("current HP is capped when it ends") {
        auto full = m.get(aria);
        full.current_hp = 36;
        m.repo.update_character(full);
        REQUIRE(dnd::end_attunement(m.repo, aria, amulet).ok);
        REQUIRE(m.get(aria).current_hp == 30);
    }
}

TEST_CASE("reading spell scrolls", "[magic_items]") {
    Magic m;
    auto spell = [&](const std::string& name) {
        for (const auto& s : m.repo.get_spells()) {
            if (s.name == name) return s;
        }
        FAIL("no spell " << name);
        return dnd::Spell{};
    };
    auto character = [&](const std::string& class_name, int level) {
        auto c = m.get(m.add(class_name + std::to_string(level), class_name));
        c.level = level;
        return c;
    };

    SECTION("a caster with the spell on their list") {
        auto reading = dnd::read_scroll(character("Wizard", 1), *dnd::find_class_info("Wizard"), spell("Magic Missile"));
        REQUIRE(reading.readable);
        REQUIRE_FALSE(reading.needs_check);
        REQUIRE(reading.save_dc == 13);
        REQUIRE(reading.attack_bonus == 5);
    }
    SECTION("not on the class's list") {
        auto reading = dnd::read_scroll(character("Fighter", 5), *dnd::find_class_info("Fighter"), spell("Bless"));
        REQUIRE_FALSE(reading.readable);
        REQUIRE(reading.reason == "Bless isn't on the Fighter spell list, so Fighter5 can't read the scroll.");
    }
    SECTION("above the reader's highest slot: a check") {
        // A level-1 Paladin has no spell slots yet.
        auto reading = dnd::read_scroll(character("Paladin", 1), *dnd::find_class_info("Paladin"), spell("Bless"));
        REQUIRE(reading.readable);
        REQUIRE(reading.needs_check);
        REQUIRE(reading.check_dc == 11);
        REQUIRE_FALSE(dnd::read_scroll(character("Paladin", 2), *dnd::find_class_info("Paladin"), spell("Bless"))
                          .needs_check);
    }
    SECTION("the DMG's scroll table") {
        REQUIRE(dnd::scroll_save_dc(0) == 13);
        REQUIRE(dnd::scroll_save_dc(3) == 15);
        REQUIRE(dnd::scroll_attack_bonus(3) == 7);
        REQUIRE(dnd::scroll_save_dc(9) == 19);
        REQUIRE(dnd::scroll_attack_bonus(9) == 11);
    }
}

TEST_CASE("shops sell common and uncommon magic only", "[magic_items][shop]") {
    Magic m;
    auto aria = m.get(m.add("Aria"));
    aria.coins = {0, 0, 0, 100000, 0};
    m.repo.update_character(aria);
    REQUIRE(dnd::sold_in_shops(m.item("Longsword")));
    REQUIRE(dnd::sold_in_shops(m.item("Longsword +1")));
    REQUIRE_FALSE(dnd::sold_in_shops(m.item("Longsword +2")));
    REQUIRE(dnd::buy(m.repo, aria.character_id, m.item("Cloak of Protection").item_id).ok);
    auto refused = dnd::buy(m.repo, aria.character_id, m.item("Amulet of Health").item_id);
    REQUIRE_FALSE(refused.ok);
    REQUIRE(refused.message == "No shop sells the Amulet of Health: rare magic is found, not bought.");
}

TEST_CASE("reading a scroll in combat", "[magic_items][encounter]") {
    Magic m;
    auto monster = [&](const std::string& name) {
        for (const auto& mon : m.repo.get_monsters()) {
            if (mon.name == name) return mon;
        }
        FAIL("no monster " << name);
        return dnd::Monster{};
    };
    auto setup = [&](dnd::Encounter& encounter, const std::string& class_name, int level, const std::string& scroll) {
        auto c = m.get(m.add(class_name, class_name));
        c.level = level;
        c.wisdom = 16;
        c.charisma = 16;
        c.intelligence = 16;
        m.repo.update_character(c);
        encounter.set_scroll_spells(m.repo.get_spells());
        encounter.add_character(c, {}, {{m.item(scroll), 1}});
        encounter.add_monsters(monster("Goblin"), 1);
        encounter.roll_initiative();
        encounter.take_events();
        return c;
    };
    auto log_text = [](const std::vector<dnd::EncounterEvent>& events) {
        std::string text;
        for (const auto& e : events) {
            if (e.kind == dnd::EncounterEvent::Kind::Log) text += e.text + "\n";
        }
        return text;
    };

    SECTION("a wizard reads Magic Missile: no slot spent, the scroll is gone") {
        // Initiative: the wizard 20, the goblin 1; then 1d4 per dart.
        ScriptedRandomSource rolls({20, 1, 2});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, m.repo.get_classes(), m.repo.get_races());
        auto zanna = setup(encounter, "Wizard", 1, "Spell Scroll (Magic Missile)");
        REQUIRE(encounter.scrolls().size() == 1);
        int scroll = m.item("Spell Scroll (Magic Missile)").item_id;
        REQUIRE(encounter.read_scroll(scroll, {1}));
        auto events = encounter.take_events();
        auto text = log_text(events);
        REQUIRE(text.find("Wizard reads a Spell Scroll (Magic Missile). Wizard casts Magic Missile.") != std::string::npos);
        REQUIRE(encounter.character(zanna.character_id)->spell_slots_used[0] == 0);
        REQUIRE(encounter.scrolls().empty());
        REQUIRE(std::any_of(events.begin(), events.end(), [&](const dnd::EncounterEvent& e) {
            return e.kind == dnd::EncounterEvent::Kind::ItemUsed && e.item_id == scroll;
        }));
    }
    SECTION("a fighter can't read a Bless scroll") {
        ScriptedRandomSource rolls({20, 1});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, m.repo.get_classes(), m.repo.get_races());
        setup(encounter, "Fighter", 3, "Spell Scroll (Bless)");
        REQUIRE_FALSE(encounter.scroll_reading(m.item("Spell Scroll (Bless)"))->readable);
        REQUIRE_FALSE(encounter.read_scroll(m.item("Spell Scroll (Bless)").item_id, {0}));
        REQUIRE(encounter.scrolls().size() == 1);  // still there
    }
    SECTION("a level-1 paladin needs a check: failing wastes the scroll") {
        // Initiative 20 / 1; the check: d20 5, CHA +3 = 8 vs DC 11.
        ScriptedRandomSource rolls({20, 1, 5});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, m.repo.get_classes(), m.repo.get_races());
        setup(encounter, "Paladin", 1, "Spell Scroll (Bless)");
        REQUIRE(encounter.read_scroll(m.item("Spell Scroll (Bless)").item_id, {0}));
        auto text = log_text(encounter.take_events());
        REQUIRE(text.find("spellcasting check d20(5) +3 vs DC 11 -> failure. The scroll crumbles to dust") !=
                std::string::npos);
        REQUIRE(encounter.scrolls().empty());
        REQUIRE_FALSE(encounter.awaiting_action());  // the action is spent
    }
    SECTION("passing the check casts it") {
        ScriptedRandomSource rolls({20, 1, 15});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, m.repo.get_classes(), m.repo.get_races());
        setup(encounter, "Paladin", 1, "Spell Scroll (Bless)");
        REQUIRE(encounter.read_scroll(m.item("Spell Scroll (Bless)").item_id, {0}));
        auto text = log_text(encounter.take_events());
        REQUIRE(text.find("-> success. Paladin casts Bless.") != std::string::npos);
    }
    SECTION("saves use the scroll's DC") {
        // Sacred Flame (Cleric cantrip): DC 13 from the scroll, whatever the cleric's own DC.
        ScriptedRandomSource rolls({20, 1, 2, 6});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, m.repo.get_classes(), m.repo.get_races());
        setup(encounter, "Cleric", 5, "Spell Scroll (Sacred Flame)");
        REQUIRE(encounter.read_scroll(m.item("Spell Scroll (Sacred Flame)").item_id, {1}));
        auto text = log_text(encounter.take_events());
        REQUIRE(text.find("vs DC 13") != std::string::npos);
    }
}

TEST_CASE("a Healer's Kit has ten uses", "[magic_items][inventory]") {
    Magic m;
    int id = m.add("Medic");
    int kit = m.item("Healer's Kit").item_id;
    REQUIRE(m.item("Healer's Kit").uses == 10);
    m.repo.add_to_inventory(id, kit, 2);

    for (int i = 0; i < 9; ++i) m.repo.spend_item_use(id, kit);
    auto pack = m.repo.get_inventory(id);
    REQUIRE(pack.front().quantity == 2);
    REQUIRE(pack.front().uses_spent == 9);
    REQUIRE(dnd::item_label(pack.front()) == "Healer's Kit (1 of 10 uses left)");

    m.repo.spend_item_use(id, kit);  // the tenth: one kit is gone, the next is fresh
    pack = m.repo.get_inventory(id);
    REQUIRE(pack.front().quantity == 1);
    REQUIRE(pack.front().uses_spent == 0);

    for (int i = 0; i < 10; ++i) m.repo.spend_item_use(id, kit);
    REQUIRE(m.repo.get_inventory(id).empty());
}

TEST_CASE("spend_use mirrors the repository", "[inventory]") {
    Magic m;
    dnd::InventoryEntry kit{m.item("Healer's Kit"), 1, false, false, 8};
    REQUIRE_FALSE(dnd::spend_use(kit));
    REQUIRE(kit.uses_spent == 9);
    REQUIRE(dnd::spend_use(kit));
    REQUIRE(kit.quantity == 0);

    dnd::InventoryEntry rope{m.item("Hempen Rope (50 feet)"), 1, false, false, 0};
    REQUIRE(dnd::spend_use(rope));  // no tracked uses: gone at once
}

TEST_CASE("reading a healing scroll between fights", "[magic_items][scrolls]") {
    Magic m;
    int cleric = m.add("Cleric", "Cleric");
    int fighter = m.add("Fighter");
    {
        auto hurt = m.get(fighter);
        hurt.current_hp = 10;
        m.repo.update_character(hurt);
    }
    int scroll = m.item("Spell Scroll (Cure Wounds)").item_id;
    m.carry(cleric, "Spell Scroll (Cure Wounds)");

    SECTION("heals the target and uses the scroll up") {
        ScriptedRandomSource rolls({5});  // 1d8
        dnd::Dice dice(rolls);
        auto result = dnd::read_scroll_outside_combat(m.repo, dice, cleric, scroll, {fighter});
        REQUIRE(result.ok);
        REQUIRE(m.get(fighter).current_hp > 10);
        REQUIRE(result.message.find("Fighter regains") != std::string::npos);
        REQUIRE(m.repo.get_inventory(cleric).empty());
    }
    SECTION("a fighter can't read it, and keeps the scroll") {
        m.carry(fighter, "Spell Scroll (Cure Wounds)");
        ScriptedRandomSource rolls({5});
        dnd::Dice dice(rolls);
        auto result = dnd::read_scroll_outside_combat(m.repo, dice, fighter, scroll, {fighter});
        REQUIRE_FALSE(result.ok);
        REQUIRE(m.repo.get_inventory(fighter).size() == 1);
        REQUIRE(m.get(fighter).current_hp == 10);
    }
    SECTION("nobody hurt: refused, scroll kept") {
        auto healed = m.get(fighter);
        healed.current_hp = healed.max_hp;
        m.repo.update_character(healed);
        ScriptedRandomSource rolls({5});
        dnd::Dice dice(rolls);
        auto result = dnd::read_scroll_outside_combat(m.repo, dice, cleric, scroll, {fighter});
        REQUIRE_FALSE(result.ok);
        REQUIRE(m.repo.get_inventory(cleric).size() == 1);
    }
    SECTION("a dead target is refused") {
        auto dead = m.get(fighter);
        dead.dead = true;
        m.repo.update_character(dead);
        ScriptedRandomSource rolls({5});
        dnd::Dice dice(rolls);
        REQUIRE_FALSE(dnd::read_scroll_outside_combat(m.repo, dice, cleric, scroll, {fighter}).ok);
    }
    SECTION("too many targets") {
        ScriptedRandomSource rolls({5});
        dnd::Dice dice(rolls);
        REQUIRE_FALSE(dnd::read_scroll_outside_combat(m.repo, dice, cleric, scroll, {fighter, cleric}).ok);
        REQUIRE_FALSE(dnd::read_scroll_outside_combat(m.repo, dice, cleric, scroll, {}).ok);
    }
    SECTION("a scroll of a combat spell has nothing to do") {
        m.carry(cleric, "Spell Scroll (Bless)");
        ScriptedRandomSource rolls({5});
        dnd::Dice dice(rolls);
        auto result = dnd::read_scroll_outside_combat(m.repo, dice, cleric, m.item("Spell Scroll (Bless)").item_id, {fighter});
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.message.find("Read it in combat") != std::string::npos);
    }
}
