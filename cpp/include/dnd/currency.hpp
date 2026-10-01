#pragma once

#include <array>
#include <string>
#include <vector>

namespace dnd {

// Coins (2014 PHB ch. 5): copper, silver, electrum, gold, platinum.
// 1 pp = 10 gp = 20 ep = 100 sp = 1,000 cp. See docs/adventures-and-currency.md.

enum class Coin { Copper, Silver, Electrum, Gold, Platinum };
constexpr int kCoinCount = 5;
using Coins = std::array<int, kCoinCount>;  // indexed by Coin

const char* coin_abbreviation(Coin coin);  // "cp", "sp", "ep", "gp", "pp"
int coin_value_cp(Coin coin);              // 1, 10, 50, 100, 1000

// Total value in copper.
int total_cp(const Coins& coins);

// `cp` worth of coins in gold, silver, and copper -- the way loot is paid
// out (no electrum or platinum).
Coins coins_from_cp(int cp);

void add_coins(Coins& purse, const Coins& more);

// "15 gp, 3 sp, 12 cp", largest first; "none" for an empty purse.
std::string format_coins(const Coins& coins);
// 1,234 cp -> "12 gp, 3 sp, 4 cp" (format_coins(coins_from_cp(cp))).
std::string format_cp(int cp);

// "cp:12,gp:15" <-> Coins, for the database (unknown or malformed parts are ignored).
std::string serialize_coins(const Coins& coins);
Coins parse_coins(const std::string& text);

// Splits `cp` evenly among `people` (>= 1): everyone gets the same, and any
// leftover copper goes one piece each to the first few.
std::vector<int> split_cp(int cp, int people);

// The gold pieces in a background's starting equipment (PHB ch. 4): 15 for
// an Acolyte, 25 for a Noble, 10 for a Soldier, ...; 0 if unknown.
int background_purse_gp(const std::string& background);

}  // namespace dnd
