#include "dnd/shop.hpp"

#include <algorithm>

#include "dnd/campaign.hpp"
#include "dnd/inventory.hpp"
#include "dnd/npc.hpp"
#include "dnd/scenes.hpp"

namespace dnd {

namespace {

std::optional<Character> find_character(const Repository& repo, int character_id) {
    for (const auto& character : repo.get_characters()) {
        if (character.character_id == character_id) return character;
    }
    return std::nullopt;
}

std::optional<Item> find_item(const Repository& repo, int item_id) {
    for (const auto& item : repo.get_items()) {
        if (item.item_id == item_id) return item;
    }
    return std::nullopt;
}

ShopResult refuse(std::string message) {
    return ShopResult{false, std::move(message)};
}

std::string stack(int quantity, const std::string& name) {
    return std::to_string(quantity) + " × " + name;
}

}  // namespace

std::optional<Coins> pay(const Coins& purse, int cost_cp) {
    if (cost_cp < 0 || total_cp(purse) < cost_cp) return std::nullopt;
    Coins left = purse;
    int remaining = cost_cp;
    // Smallest first, never overpaying with a coin.
    for (int i = 0; i < kCoinCount && remaining > 0; ++i) {
        auto& count = left[static_cast<size_t>(i)];
        int value = coin_value_cp(static_cast<Coin>(i));
        int used = std::min(count, remaining / value);
        count -= used;
        remaining -= used * value;
    }
    // Every coin still held is now worth more than what's owed: break the
    // smallest one.
    if (remaining > 0) {
        for (int i = 0; i < kCoinCount; ++i) {
            auto& count = left[static_cast<size_t>(i)];
            if (count == 0) continue;
            --count;
            add_coins(left, coins_from_cp(coin_value_cp(static_cast<Coin>(i)) - remaining));
            break;
        }
    }
    return left;
}

bool sold_in_shops(const Item& item) {
    return item.rarity == Rarity::None || item.rarity == Rarity::Common || item.rarity == Rarity::Uncommon;
}

int sell_price_cp(const Item& item) {
    return std::max(0, item.cost_cp / 2);
}

int raise_dead_cost_cp(Repository& repo, int campaign_id) {
    return temple_priest(repo, campaign_id).attitude == Attitude::Friendly ? kRaiseDeadFriendlyCostCp
                                                                          : kRaiseDeadCostCp;
}

int price_for(const Character& character, int price_cp) {
    if (character.haggle <= 0) return price_cp;
    return price_cp - price_cp * kHaggleDiscountPercent / 100;  // the discount rounds down
}

ShopResult haggle(Repository& repo, int character_id, const Dice& dice) {
    auto character = find_character(repo, character_id);
    if (!character) return refuse("That character no longer exists.");
    if (character->dead) return refuse(character->character_name + " is dead.");
    if (character->current_hp <= 0) return refuse(character->character_name + " is unconscious.");
    if (character->haggle != 0) {
        return refuse(character->character_name + " has already haggled today. Try again after a long rest.");
    }
    Npc merchant = shop_merchant(repo, character->campaign_id);
    auto dc = conversation_dc(merchant, SocialRequest::MinorRisk);
    if (!dc) return refuse(merchant.name + " is Hostile and won't haggle with you.");
    auto rollers = party_rollers(repo, {character_id});
    if (rollers.empty()) return refuse(character->character_name + " can't haggle now.");
    CheckResult check = ability_check(dice, rollers.front(), "Persuasion", *dc);
    character->haggle = check.success ? 1 : -1;
    repo.update_character(*character);
    return ShopResult{check.success,
                      check.text() + ". " +
                          (check.success ? merchant.name + " knocks " + std::to_string(kHaggleDiscountPercent) +
                                               "% off for " + character->character_name + " until their next long rest."
                                         : merchant.name + " won't budge on prices today.")};
}

ShopResult buy(Repository& repo, int character_id, int item_id, int quantity) {
    if (quantity <= 0) return refuse("Choose how many to buy.");
    auto character = find_character(repo, character_id);
    auto item = find_item(repo, item_id);
    if (!character || !item) return refuse("That character or item no longer exists.");
    if (character->dead) return refuse(character->character_name + " is dead and can't go shopping.");
    if (!sold_in_shops(*item)) return refuse("No shop sells the " + item->name + ": rare magic is found, not bought.");

    int price = price_for(*character, item->cost_cp * quantity);
    auto paid = pay(character->coins, price);
    if (!paid) {
        return refuse(stack(quantity, item->name) + " costs " + format_cost(price) + ", but " +
                      character->character_name + " has " + format_cost(total_cp(character->coins)) + ".");
    }
    character->coins = *paid;
    repo.with_transaction([&] {
        repo.update_character(*character);
        repo.add_to_inventory(character_id, item_id, quantity);
    });
    return ShopResult{true, character->character_name + " bought " + stack(quantity, item->name) + " for " +
                                format_cost(price) + "."};
}

ShopResult sell(Repository& repo, int character_id, int item_id, int quantity) {
    if (quantity <= 0) return refuse("Choose how many to sell.");
    auto character = find_character(repo, character_id);
    if (!character) return refuse("That character no longer exists.");
    auto inventory = repo.get_inventory(character_id);
    auto entry = std::find_if(inventory.begin(), inventory.end(),
                              [item_id](const InventoryEntry& e) { return e.item.item_id == item_id; });
    if (entry == inventory.end()) return refuse(character->character_name + " doesn't have that item.");
    const Item& item = entry->item;
    if (entry->equipped) return refuse("Unequip the " + item.name + " in the Inventory before selling it.");
    if (entry->attuned) return refuse("End attunement to the " + item.name + " (a short rest) before selling it.");
    if (quantity > entry->quantity) {
        return refuse(character->character_name + " has only " + stack(entry->quantity, item.name) + ".");
    }
    int each = sell_price_cp(item);
    if (each == 0) return refuse("No one will pay anything for a " + item.name + ".");

    int price = each * quantity;
    add_coins(character->coins, coins_from_cp(price));
    repo.with_transaction([&] {
        repo.remove_from_inventory(character_id, item_id, quantity);
        repo.update_character(*character);
    });
    return ShopResult{true, character->character_name + " sold " + stack(quantity, item.name) + " for " +
                                format_cost(price) + "."};
}

ShopResult raise_dead(Repository& repo, int character_id, int payer_id) {
    auto character = find_character(repo, character_id);
    auto payer = find_character(repo, payer_id);
    if (!character || !payer) return refuse("That character no longer exists.");
    if (!character->dead) return refuse(character->character_name + " isn't dead.");
    if (!in_play(repo, *payer)) return refuse(payer->character_name + " isn't in this campaign.");
    bool self_paid = payer_id == character_id;
    int cost = raise_dead_cost_cp(repo, character->campaign_id);
    auto paid = pay(payer->coins, cost);
    if (!paid) {
        return refuse("Raise dead costs " + format_cost(cost) + ", but " + payer->character_name +
                      " has " + format_cost(total_cp(payer->coins)) + ".");
    }
    if (self_paid) {
        character->coins = *paid;
    } else {
        payer->coins = *paid;
    }
    character->dead = false;
    character->current_hp = 1;
    character->revival_penalty = kRaiseDeadPenalty;
    repo.with_transaction([&] {
        repo.update_character(*character);
        if (!self_paid) repo.update_character(*payer);
    });
    std::string name = character->character_name;
    return ShopResult{true, "The temple's priests raise " + name + " from the dead, for " +
                                format_cost(cost) + " from " + (self_paid ? name + "'s own purse" : payer->character_name) +
                                ". " + name + " returns with 1 HP and -" + std::to_string(kRaiseDeadPenalty) +
                                " to attack rolls, saving throws, and ability checks, easing by 1 each long rest."};
}

ShopResult give_coins(Repository& repo, int from_id, int to_id, const Coins& coins) {
    if (from_id == to_id) return refuse("Choose someone else to give the coins to.");
    auto from = find_character(repo, from_id);
    auto to = find_character(repo, to_id);
    if (!from || !to) return refuse("That character no longer exists.");
    if (to->dead) return refuse(to->character_name + " is dead.");
    if (!in_play(repo, *to)) return refuse(to->character_name + " isn't in this campaign.");
    if (total_cp(coins) <= 0) return refuse("Choose some coins to give.");
    for (int i = 0; i < kCoinCount; ++i) {
        size_t slot = static_cast<size_t>(i);
        if (coins[slot] < 0) return refuse("Coin counts can't be negative.");
        if (coins[slot] > from->coins[slot]) {
            return refuse(from->character_name + " has only " + std::to_string(from->coins[slot]) + " " +
                          coin_abbreviation(static_cast<Coin>(i)) + ".");
        }
    }
    for (size_t i = 0; i < coins.size(); ++i) {
        from->coins[i] -= coins[i];
        to->coins[i] += coins[i];
    }
    repo.with_transaction([&] {
        repo.update_character(*from);
        repo.update_character(*to);
    });
    return ShopResult{true, from->character_name + " gave " + to->character_name + " " + format_coins(coins) + "."};
}

ShopResult give_item(Repository& repo, int from_id, int to_id, int item_id, int quantity) {
    if (quantity <= 0) return refuse("Choose how many to give.");
    if (from_id == to_id) return refuse("Choose someone else to give it to.");
    auto from = find_character(repo, from_id);
    auto to = find_character(repo, to_id);
    if (!from || !to) return refuse("That character no longer exists.");
    if (to->dead) return refuse(to->character_name + " is dead.");
    if (!in_play(repo, *to)) return refuse(to->character_name + " isn't in this campaign.");
    auto inventory = repo.get_inventory(from_id);
    auto entry = std::find_if(inventory.begin(), inventory.end(),
                              [item_id](const InventoryEntry& e) { return e.item.item_id == item_id; });
    if (entry == inventory.end()) return refuse(from->character_name + " doesn't have that item.");
    const Item& item = entry->item;
    if (entry->equipped) return refuse("Unequip the " + item.name + " before giving it away.");
    if (entry->attuned) return refuse("End attunement to the " + item.name + " (a short rest) before giving it away.");
    if (quantity > entry->quantity) {
        return refuse(from->character_name + " has only " + stack(entry->quantity, item.name) + ".");
    }
    repo.with_transaction([&] {
        repo.remove_from_inventory(from_id, item_id, quantity);
        repo.add_to_inventory(to_id, item_id, quantity);
    });
    return ShopResult{true, from->character_name + " gave " + to->character_name + " " + stack(quantity, item.name) + "."};
}

}  // namespace dnd
