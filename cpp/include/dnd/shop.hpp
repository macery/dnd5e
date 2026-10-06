#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dnd/currency.hpp"
#include "dnd/dice.hpp"
#include "dnd/item.hpp"
#include "dnd/repository.hpp"

namespace dnd {

// Buying, selling, raising the dead, and handing coins and items over
// (docs/shops.md). Any character can pay for or trade with any other,
// whoever's player they are -- within the active campaign, when there is
// one (dnd/campaign.hpp).
//
// Prices are the PHB ch. 5 list prices (Item::cost_cp).

// The purse after paying `cost_cp`, or nothing if it holds less. The
// smallest coins go first, and a larger coin is broken only when it has to
// be, with the change in gold, silver, and copper: paying 2 sp from 1 gp
// leaves 8 sp. Coins that aren't needed are left alone.
std::optional<Coins> pay(const Coins& purse, int cost_cp);

// Whether shops sell it: mundane gear, and common and uncommon magic
// items. Rarer magic comes only as loot (docs/magic-items.md).
bool sold_in_shops(const Item& item);

// What a shop pays for one: half the list price, rounded down (PHB ch. 5,
// "Selling Treasure"). 0 means it can't be sold.
int sell_price_cp(const Item& item);

// *Raise dead* at a temple: the 500 gp diamond the spell consumes plus a
// 50 gp fee (the top of the PHB's 10-50 gp for spellcasting services).
constexpr int kRaiseDeadCostCp = 55000;
// A Friendly priest waives the fee: just the diamond.
constexpr int kRaiseDeadFriendlyCostCp = 50000;
// What raising someone in `campaign_id` costs, by its priest's attitude.
int raise_dead_cost_cp(Repository& repo, int campaign_id);

// Haggling (docs/scenes-and-npcs.md): once between long rests, a character
// can make a Persuasion check against the merchant of their campaign. The
// DC is the DMG's for a minor sacrifice (10 if the merchant is Friendly, 20 if
// Indifferent, 5 less once their wants and fears are known); a Hostile
// merchant won't haggle. Success takes this much off everything they buy
// until their next long rest.
constexpr int kHaggleDiscountPercent = 10;
// The price after the character's discount, if they have one (rounded up).
int price_for(const Character& character, int price_cp);
// The penalty a raised character starts with (PHB ch. 11, *raise dead*).
constexpr int kRaiseDeadPenalty = 4;

// The outcome of a shop action: `ok`, and a line for the player either way
// ("Bought 2 × Torch for 2 cp." or why not).
struct ShopResult {
    bool ok = false;
    std::string message;
};

// Each of these changes the database in one transaction, or not at all.

// Rolls the haggle and saves the result on the character.
ShopResult haggle(Repository& repo, int character_id, const Dice& dice);

// Pays for `quantity` (> 0) of the item (at the haggled price, if any) and
// adds them to the inventory.
ShopResult buy(Repository& repo, int character_id, int item_id, int quantity = 1);

// Removes `quantity` (> 0) of the item and pays the sale price. Equipped or
// attuned items can't be sold, and neither can items worth nothing.
ShopResult sell(Repository& repo, int character_id, int item_id, int quantity = 1);

// Charges `payer_id` (any character, the dead one included)
// raise_dead_cost_cp and brings `character_id` back with 1 HP and
// the *raise dead* penalty.
ShopResult raise_dead(Repository& repo, int character_id, int payer_id);

// Moves `coins` from one character's purse to another's, coin for coin. The
// giver may be dead (looting a fallen friend); the receiver may not.
ShopResult give_coins(Repository& repo, int from_id, int to_id, const Coins& coins);

// Moves `quantity` (> 0) of an item from one character's pack to another's.
// Equipped or attuned items can't be given. Living receivers only, as above.
ShopResult give_item(Repository& repo, int from_id, int to_id, int item_id, int quantity = 1);

}  // namespace dnd
