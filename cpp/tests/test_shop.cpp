#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "dnd/character_io.hpp"
#include "dnd/encounter.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"
#include "dnd/shop.hpp"

using dnd::Coins;

namespace {

// cp, sp, ep, gp, pp
Coins coins(int cp, int sp, int ep, int gp, int pp) {
    return Coins{cp, sp, ep, gp, pp};
}

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

struct Shop {
    dnd::Repository repo{":memory:"};
    int player_id = repo.add_player("Adam");

    int add(const std::string& name, const Coins& purse, bool dead = false) {
        dnd::Character c;
        c.character_name = name;
        c.class_id = class_id("Fighter");
        c.race = repo.get_races().front().id;
        c.alignment = repo.get_alignments().front().id;
        c.player_id = player_id;
        c.strength = 16;
        c.max_hp = 12;
        c.current_hp = dead ? 0 : 12;
        c.dead = dead;
        c.coins = purse;
        return repo.add_character(c);
    }

    int class_id(const std::string& name) {
        for (const auto& c : repo.get_classes()) {
            if (c.name == name) return c.id;
        }
        FAIL("no class " << name);
        return 0;
    }

    dnd::Item item(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i;
        }
        FAIL("no item " << name);
        return {};
    }

    dnd::Character get(int id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == id) return c;
        }
        FAIL("no character " << id);
        return {};
    }

    int count(int character_id, const std::string& item_name) {
        for (const auto& entry : repo.get_inventory(character_id)) {
            if (entry.item.name == item_name) return entry.quantity;
        }
        return 0;
    }
};

}  // namespace

TEST_CASE("paying makes change", "[shop]") {
    SECTION("exact coins") {
        REQUIRE(dnd::pay(coins(5, 3, 0, 2, 0), 235) == coins(0, 0, 0, 0, 0));
    }
    SECTION("breaking a gold piece for 2 sp leaves 8 sp") {
        REQUIRE(dnd::pay(coins(0, 0, 0, 1, 0), 20) == coins(0, 8, 0, 0, 0));
    }
    SECTION("smallest coins first; larger ones untouched when not needed") {
        REQUIRE(dnd::pay(coins(50, 0, 0, 3, 1), 30) == coins(20, 0, 0, 3, 1));
        REQUIRE(dnd::pay(coins(0, 0, 0, 5, 2), 500) == coins(0, 0, 0, 0, 2));
    }
    SECTION("coppers that can't cover it go first, then a coin is broken") {
        // 5 cp + 1 gp for 2 sp: the 5 cp, then 15 cp out of the gold piece.
        REQUIRE(dnd::pay(coins(5, 0, 0, 1, 0), 20) == coins(5, 8, 0, 0, 0));
    }
    SECTION("electrum and platinum are spent and broken like any coin") {
        REQUIRE(dnd::pay(coins(0, 0, 3, 0, 0), 120) == coins(0, 3, 0, 0, 0));
        REQUIRE(dnd::pay(coins(0, 0, 0, 0, 1), 1) == coins(9, 9, 0, 9, 0));
    }
    SECTION("never more than the purse holds") {
        REQUIRE_FALSE(dnd::pay(coins(9, 9, 0, 0, 0), 100).has_value());
        REQUIRE_FALSE(dnd::pay(coins(0, 0, 0, 0, 0), 1).has_value());
        REQUIRE(dnd::pay(coins(0, 0, 0, 0, 0), 0) == coins(0, 0, 0, 0, 0));
    }
    SECTION("the value always drops by exactly the price") {
        Coins purse = coins(7, 3, 2, 4, 1);
        for (int price = 0; price <= dnd::total_cp(purse); price += 37) {
            auto left = dnd::pay(purse, price);
            REQUIRE(left.has_value());
            REQUIRE(dnd::total_cp(*left) == dnd::total_cp(purse) - price);
            for (int count : *left) REQUIRE(count >= 0);
        }
    }
}

TEST_CASE("selling is half price, rounded down", "[shop]") {
    Shop shop;
    REQUIRE(dnd::sell_price_cp(shop.item("Longsword")) == 750);         // 15 gp
    REQUIRE(dnd::sell_price_cp(shop.item("Potion of Healing")) == 2500);  // 50 gp
    dnd::Item penny;
    penny.cost_cp = 1;
    REQUIRE(dnd::sell_price_cp(penny) == 0);
}

TEST_CASE("buying and selling", "[shop]") {
    Shop shop;
    int aria = shop.add("Aria", coins(0, 0, 0, 20, 0));
    int longsword = shop.item("Longsword").item_id;

    auto bought = dnd::buy(shop.repo, aria, longsword);
    REQUIRE(bought.ok);
    REQUIRE(bought.message == "Aria bought 1 × Longsword for 15 gp.");
    REQUIRE(shop.get(aria).coins == coins(0, 0, 0, 5, 0));
    REQUIRE(shop.count(aria, "Longsword") == 1);

    SECTION("too expensive: nothing changes") {
        auto refused = dnd::buy(shop.repo, aria, longsword);
        REQUIRE_FALSE(refused.ok);
        REQUIRE(refused.message == "1 × Longsword costs 15 gp, but Aria has 5 gp.");
        REQUIRE(shop.get(aria).coins == coins(0, 0, 0, 5, 0));
        REQUIRE(shop.count(aria, "Longsword") == 1);
    }
    SECTION("quantities multiply the price") {
        auto torches = dnd::buy(shop.repo, aria, shop.item("Torch").item_id, 10);
        REQUIRE(torches.ok);
        REQUIRE(torches.message == "Aria bought 10 × Torch for 1 sp.");
        REQUIRE(shop.get(aria).coins == coins(0, 9, 0, 4, 0));
        REQUIRE_FALSE(dnd::buy(shop.repo, aria, longsword, 0).ok);
    }
    SECTION("selling pays half") {
        auto sold = dnd::sell(shop.repo, aria, longsword);
        REQUIRE(sold.ok);
        REQUIRE(sold.message == "Aria sold 1 × Longsword for 7 gp 5 sp.");
        REQUIRE(shop.get(aria).coins == coins(0, 5, 0, 12, 0));
        REQUIRE(shop.count(aria, "Longsword") == 0);
        REQUIRE_FALSE(dnd::sell(shop.repo, aria, longsword).ok);  // none left
    }
    SECTION("equipped gear can't be sold") {
        shop.repo.set_equipped(aria, longsword, true);
        auto refused = dnd::sell(shop.repo, aria, longsword);
        REQUIRE_FALSE(refused.ok);
        REQUIRE(refused.message == "Unequip the Longsword in the Inventory before selling it.");
        REQUIRE(shop.count(aria, "Longsword") == 1);
    }
    SECTION("no more than the character has") {
        REQUIRE_FALSE(dnd::sell(shop.repo, aria, longsword, 2).ok);
    }
    SECTION("the dead don't shop") {
        int ghost = shop.add("Ghost", coins(0, 0, 0, 100, 0), true);
        REQUIRE_FALSE(dnd::buy(shop.repo, ghost, longsword).ok);
    }
}

TEST_CASE("raising the dead", "[shop]") {
    Shop shop;
    int bram = shop.add("Bram", coins(0, 0, 0, 0, 0), true);
    int cleo = shop.add("Cleo", coins(0, 0, 0, 600, 0));

    SECTION("a friend pays") {
        auto raised = dnd::raise_dead(shop.repo, bram, cleo);
        REQUIRE(raised.ok);
        auto bram_now = shop.get(bram);
        REQUIRE_FALSE(bram_now.dead);
        REQUIRE(bram_now.current_hp == 1);
        REQUIRE(bram_now.revival_penalty == 4);
        REQUIRE(shop.get(cleo).coins == coins(0, 0, 0, 50, 0));
        REQUIRE_FALSE(dnd::raise_dead(shop.repo, bram, cleo).ok);  // not dead any more
    }
    SECTION("the dead character's own purse pays") {
        int dax = shop.add("Dax", coins(0, 0, 0, 0, 55), true);
        auto raised = dnd::raise_dead(shop.repo, dax, dax);
        REQUIRE(raised.ok);
        REQUIRE(raised.message.find("Dax's own purse") != std::string::npos);
        REQUIRE(shop.get(dax).coins == coins(0, 0, 0, 0, 0));
        REQUIRE_FALSE(shop.get(dax).dead);
    }
    SECTION("not enough money") {
        auto refused = dnd::raise_dead(shop.repo, bram, bram);
        REQUIRE_FALSE(refused.ok);
        REQUIRE(refused.message == "Raise dead costs 550 gp, but Bram has 0 cp.");
        REQUIRE(shop.get(bram).dead);
    }
    SECTION("another player's character can pay") {
        int other_player = shop.repo.add_player("Beth");
        dnd::Character rich = shop.get(cleo);
        rich.player_id = other_player;
        rich.character_name = "Rich";
        int rich_id = shop.repo.add_character(rich);
        REQUIRE(dnd::raise_dead(shop.repo, bram, rich_id).ok);
    }
    SECTION("the penalty eases by 1 per long rest") {
        REQUIRE(dnd::raise_dead(shop.repo, bram, cleo).ok);
        auto c = shop.get(bram);
        for (int expected : {3, 2, 1, 0, 0}) {
            dnd::take_long_rest(c);
            REQUIRE(c.revival_penalty == expected);
        }
    }
}

TEST_CASE("giving coins", "[shop]") {
    Shop shop;
    int aria = shop.add("Aria", coins(10, 0, 2, 30, 0));
    int bram = shop.add("Bram", coins(0, 0, 0, 1, 0));

    auto given = dnd::give_coins(shop.repo, aria, bram, coins(0, 0, 2, 25, 0));
    REQUIRE(given.ok);
    REQUIRE(given.message == "Aria gave Bram 25 gp, 2 ep.");
    REQUIRE(shop.get(aria).coins == coins(10, 0, 0, 5, 0));
    REQUIRE(shop.get(bram).coins == coins(0, 0, 2, 26, 0));

    REQUIRE_FALSE(dnd::give_coins(shop.repo, aria, bram, coins(0, 0, 0, 6, 0)).ok);  // only 5 gp
    REQUIRE_FALSE(dnd::give_coins(shop.repo, aria, bram, coins(0, 0, 0, 0, 0)).ok);
    REQUIRE_FALSE(dnd::give_coins(shop.repo, aria, aria, coins(1, 0, 0, 0, 0)).ok);
    REQUIRE(shop.get(aria).coins == coins(10, 0, 0, 5, 0));

    SECTION("from the dead, but not to them") {
        int ghost = shop.add("Ghost", coins(0, 0, 0, 7, 0), true);
        REQUIRE(dnd::give_coins(shop.repo, ghost, aria, coins(0, 0, 0, 7, 0)).ok);
        auto refused = dnd::give_coins(shop.repo, aria, ghost, coins(0, 0, 0, 1, 0));
        REQUIRE_FALSE(refused.ok);
        REQUIRE(refused.message == "Ghost is dead.");
    }
    SECTION("across players") {
        dnd::Character other = shop.get(bram);
        other.player_id = shop.repo.add_player("Beth");
        other.character_name = "Phi";
        int phi = shop.repo.add_character(other);
        REQUIRE(dnd::give_coins(shop.repo, aria, phi, coins(10, 0, 0, 0, 0)).ok);
    }
}

TEST_CASE("giving items", "[shop]") {
    Shop shop;
    int clark = shop.add("Clark", {});
    shop.player_id = shop.repo.add_player("Beth");  // Phi belongs to another player
    int phi = shop.add("Phi", {});
    int potion = shop.item("Potion of Healing").item_id;
    int longsword = shop.item("Longsword").item_id;
    shop.repo.add_to_inventory(clark, potion, 2);
    shop.repo.add_to_inventory(clark, longsword);
    shop.repo.set_equipped(clark, longsword, true);

    auto given = dnd::give_item(shop.repo, clark, phi, potion);
    REQUIRE(given.ok);
    REQUIRE(given.message == "Clark gave Phi 1 × Potion of Healing.");
    REQUIRE(shop.count(clark, "Potion of Healing") == 1);
    REQUIRE(shop.count(phi, "Potion of Healing") == 1);

    REQUIRE(dnd::give_item(shop.repo, clark, phi, potion).ok);  // the last one; it stacks
    REQUIRE(shop.count(clark, "Potion of Healing") == 0);
    REQUIRE(shop.count(phi, "Potion of Healing") == 2);

    auto equipped = dnd::give_item(shop.repo, clark, phi, longsword);
    REQUIRE_FALSE(equipped.ok);
    REQUIRE(equipped.message == "Unequip the Longsword before giving it away.");
    REQUIRE(shop.count(clark, "Longsword") == 1);

    REQUIRE_FALSE(dnd::give_item(shop.repo, clark, phi, potion).ok);         // none left
    REQUIRE_FALSE(dnd::give_item(shop.repo, phi, phi, potion).ok);           // to themselves
    REQUIRE_FALSE(dnd::give_item(shop.repo, phi, clark, potion, 3).ok);      // more than they have
    REQUIRE_FALSE(dnd::give_item(shop.repo, phi, clark, potion, 0).ok);
}

TEST_CASE("the raise dead penalty in combat", "[shop][encounter]") {
    Shop shop;
    dnd::Character aria = shop.get(shop.add("Aria", {}));
    aria.revival_penalty = 4;
    std::vector<dnd::InventoryEntry> gear = {{shop.item("Longsword"), 1, true}};

    SECTION("initiative and attacks") {
        // Initiative: Aria 20 (+0 DEX -4 = 16), Goblin 1. Aria's attack: d20 15.
        ScriptedRandomSource rolls({20, 1, 15, 8});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, shop.repo.get_classes(), shop.repo.get_races());
        encounter.add_character(aria, {}, gear);
        encounter.add_monsters(shop.repo.get_monsters().front(), 1);
        const auto& me = encounter.combatants()[0];
        REQUIRE(me.roll_penalty == 4);
        encounter.roll_initiative();
        REQUIRE(encounter.combatants()[0].initiative == 16);
        encounter.take_events();
        encounter.attack(1, me.attack_bonus, me.damage_dice_count, me.damage_dice_sides, me.damage_bonus);
        bool noted = false;
        for (const auto& event : encounter.take_events()) {
            if (event.text.find("d20(15) +5 -4 (raise dead)") != std::string::npos) noted = true;
        }
        REQUIRE(noted);
    }
    SECTION("saving throws") {
        // Initiative: Aria 1, Wolf 20. The wolf bites (pick, d20 19, 2d4) and
        // Aria saves against being knocked prone: d20 10, STR +3 -4 = -1.
        ScriptedRandomSource rolls({1, 20, 1, 19, 2, 2, 10});
        dnd::Dice dice(rolls);
        dnd::Encounter encounter(dice, shop.repo.get_classes(), shop.repo.get_races());
        encounter.add_character(aria, {}, gear);
        for (const auto& monster : shop.repo.get_monsters()) {
            if (monster.name == "Wolf") encounter.add_monsters(monster, 1);
        }
        encounter.roll_initiative();
        bool noted = false;
        for (const auto& event : encounter.take_events()) {
            if (event.text.find("STR save d20(10) -1") != std::string::npos) noted = true;
        }
        REQUIRE(noted);
    }
    SECTION("death saves") {
        dnd::DeathSaveState state;
        ScriptedRandomSource rolls({12});
        dnd::Dice dice(rolls);
        dnd::roll_death_save(dice, state, -4);  // 12 - 4 = 8: a failure
        REQUIRE(state.failures == 1);
        dnd::roll_death_save(dice, state);  // 12: a success
        REQUIRE(state.successes == 1);
    }
}

TEST_CASE("the raise dead penalty survives export and import", "[shop][character_io]") {
    Shop shop;
    int aria = shop.add("Aria", {});
    auto c = shop.get(aria);
    c.revival_penalty = 2;
    shop.repo.update_character(c);
    REQUIRE(shop.get(aria).revival_penalty == 2);

    auto file = dnd::export_character(shop.repo, aria);
    auto parsed = dnd::parse_character_file(dnd::to_json(file));
    REQUIRE(parsed.file.has_value());
    REQUIRE(parsed.file->characters.at(0).revival_penalty == 2);
}
