#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "dnd/currency.hpp"

using dnd::Coins;

TEST_CASE("coin values and totals", "[currency]") {
    REQUIRE(dnd::coin_value_cp(dnd::Coin::Platinum) == 1000);
    REQUIRE(dnd::coin_value_cp(dnd::Coin::Electrum) == 50);
    Coins purse{12, 3, 2, 15, 1};  // cp, sp, ep, gp, pp
    REQUIRE(dnd::total_cp(purse) == 12 + 30 + 100 + 1500 + 1000);
    REQUIRE(dnd::coins_from_cp(1234) == Coins{4, 3, 0, 12, 0});  // gold, silver, copper only
    REQUIRE(dnd::coins_from_cp(-5) == Coins{});
}

TEST_CASE("formatting coins", "[currency]") {
    REQUIRE(dnd::format_coins(Coins{12, 3, 0, 15, 0}) == "15 gp, 3 sp, 12 cp");
    REQUIRE(dnd::format_coins(Coins{0, 0, 2, 0, 1}) == "1 pp, 2 ep");
    REQUIRE(dnd::format_coins(Coins{}) == "none");
    REQUIRE(dnd::format_cp(800) == "8 gp");
}

TEST_CASE("coins are stored as text", "[currency]") {
    Coins purse{12, 0, 0, 15, 1};
    REQUIRE(dnd::serialize_coins(purse) == "cp:12,gp:15,pp:1");
    REQUIRE(dnd::parse_coins("cp:12,gp:15,pp:1") == purse);
    REQUIRE(dnd::parse_coins("") == Coins{});
    REQUIRE(dnd::parse_coins("gp:x,zz:4,sp:3") == Coins{0, 3, 0, 0, 0});  // bad parts ignored
}

TEST_CASE("splitting coins among the party", "[currency]") {
    REQUIRE(dnd::split_cp(900, 3) == std::vector<int>{300, 300, 300});
    REQUIRE(dnd::split_cp(1001, 3) == std::vector<int>{334, 334, 333});  // leftover copper to the first
    REQUIRE(dnd::split_cp(5, 0) == std::vector<int>{5});
}

TEST_CASE("background purses (PHB ch. 4)", "[currency]") {
    REQUIRE(dnd::background_purse_gp("Soldier") == 10);
    REQUIRE(dnd::background_purse_gp("Acolyte") == 15);
    REQUIRE(dnd::background_purse_gp("Noble") == 25);
    REQUIRE(dnd::background_purse_gp("Hermit") == 5);
    REQUIRE(dnd::background_purse_gp("Pirate King") == 0);
}
