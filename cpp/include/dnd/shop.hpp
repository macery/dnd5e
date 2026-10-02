#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dnd/currency.hpp"
#include "dnd/item.hpp"
#include "dnd/repository.hpp"

namespace dnd {

// Buying, selling, raising the dead, and handing coins over (docs/shops.md).
// Prices are the PHB ch. 5 list prices (Item::cost_cp).

// The purse after paying `cost_cp`, or nothing if it holds less. The
// smallest coins go first, and a larger coin is broken only when it has to
// be, with the change in gold, silver, and copper: paying 2 sp from 1 gp
// leaves 8 sp. Coins that aren't needed are left alone.
std::optional<Coins> pay(const Coins& purse, int cost_cp);

// What a shop pays for one: half the list price, rounded down (PHB ch. 5,
// "Selling Treasure"). 0 means it can't be sold.
int sell_price_cp(const Item& item);

// *Raise dead* at a temple: the 500 gp diamond the spell consumes plus a
// 50 gp fee (the top of the PHB's 10-50 gp for spellcasting services).
constexpr int kRaiseDeadCostCp = 55000;
// The penalty a raised character starts with (PHB ch. 11, *raise dead*).
constexpr int kRaiseDeadPenalty = 4;

// The outcome of a shop action: `ok`, and a line for the player either way
// ("Bought 2 × Torch for 2 cp." or why not).
struct ShopResult {
    bool ok = false;
    std::string message;
};

// Each of these changes the database in one transaction, or not at all.

// Pays for `quantity` (> 0) of the item and adds them to the inventory.
ShopResult buy(Repository& repo, int character_id, int item_id, int quantity = 1);

// Removes `quantity` (> 0) of the item and pays the sale price. Equipped
// items can't be sold, and neither can items worth nothing.
ShopResult sell(Repository& repo, int character_id, int item_id, int quantity = 1);

// Charges `payer_id` (any character of the same player, the dead one
// included) kRaiseDeadCostCp and brings `character_id` back with 1 HP and
// the *raise dead* penalty.
ShopResult raise_dead(Repository& repo, int character_id, int payer_id);

// Moves `coins` from one character's purse to another's, coin for coin.
// Both must belong to the same player.
ShopResult give_coins(Repository& repo, int from_id, int to_id, const Coins& coins);

}  // namespace dnd
