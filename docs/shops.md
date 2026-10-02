# Shops and spending design

Status: **phases 1 (rules) and 2 (UI) done.** Decisions were settled on 2026-10-02; see [Decisions](#decisions).

**Phase 2 as built:**
- **`ShopDialog`**, from the main window's **Shop…** button, for the selected character:
  - **Header:** name and purse, and **Give Coins…** (`GiveCoinsDialog`: a recipient among the player's other characters, and a count per coin, capped at what the giver has).
  - **Buy:** every item, filterable by category. The button shows the total ("Buy for 4 gp") and is disabled with the shortfall in its tooltip when the purse is short, or for the dead. Proficiency and Strength warnings show for the selected item.
  - **Sell:** the pack with what each fetches. Equipped items are marked, and Sell is disabled with "Unequip the Longsword in the Inventory before selling it."
  - **Temple:** the player's dead and who pays (each with their purse). A dead character opens the Shop on this tab.
- **Inventory:** **Add (free)**, with a tooltip pointing to the Shop.
- **Character sheet:** a **Raised** row while the penalty lasts, and the dead banner points to the Temple.
- **UI tests** (`tests/ui/test_shop_ui.cpp`): buying with warnings and prices, refusing equipped gear, pooling coins with Give Coins to raise the dead, and the sheet's penalty row. The Shop and Give Coins are in the small-screen size test.

**Phase 1 as built:**
- **`dnd/shop`:**
  - `pay` spends the smallest coins first and breaks one larger coin for change (gold, silver, copper).
  - `sell_price_cp` is half price.
  - `buy`, `sell` (refused for equipped items), `raise_dead` (550 gp, any of the player's characters pays), and `give_coins`. Each runs in one transaction and returns a message line.
- **The *raise dead* penalty:**
  - Migration 22 adds `Characters.RevivalPenalty` (`Character::revival_penalty`). It starts at 4, and `take_long_rest` lowers it by 1.
  - In combat it comes off initiative, weapon and spell attack rolls, saving throws (concentration included), and the 10+ test of death saves.
  - Weapon attacks and death saves log it: "d20(15) +5 -4 (raise dead)".
  - Exports carry it as an optional `revival_penalty`, and the editor keeps it.
- **`equipment_warnings`:** the proficiency and Strength warnings from `equip`, shared with the shop.

## Where things stand

- **Coins pile up with nothing to spend them on.** Characters start with their background's purse, adventures pay coins split among the survivors, and the sheet and Inventory show them.
- **Every item has a price** (`Item.cost_cp`, from the PHB ch. 5 tables): 32 seeded items, from rations to plate armor.
- **Items are free.** The Inventory dialog's **Add** button puts any item in the pack at no cost.
- **Death is permanent.** A character who fails three death saves stays dead. The only way back is the editor, and it keeps the dead flag.

## Goals

- **Buy gear at list price.** The cost comes out of the buyer's purse, with change made automatically.
- **Sell gear for half its price,** as the PHB suggests for arms, armor, and other equipment (ch. 5, "Selling Treasure").
- **A temple that raises the dead** with *raise dead*, for a price (question 4). A dead character returns with 1 hit point (PHB ch. 11).
- **Warnings before buying something the character can't use well:** armor they aren't proficient with, or armor whose Strength requirement they don't meet. These reuse the equipment checks.
- **Tests:** unit tests for paying and change; an offscreen UI test that buys, sells, and raises a character.

## Non-goals for now

- **Limited stock, haggling, and town-by-town prices.** Every shop sells everything at list price.
- **Other spellcasting services** (*cure wounds*, *remove curse*, ...). Long rests are free on the main window, so paid healing would buy nothing.
- **Lifestyle expenses and inns.** Same reason: resting doesn't cost anything yet.
- **Magic items.** They're their own TODO.
- **Time limits on raising the dead.** The app has no calendar, so the "dead no longer than 10 days" limit isn't checked.
- **Encumbrance**, including the weight of coins.

## Rules (pure functions in `dndcore`, with tests)

A new module, `dnd/shop`:

- **`pay(purse, cost_cp)`:** returns the purse after paying, or nothing if it holds less than the cost.
  - Spends the smallest coins first, and breaks a larger coin only when it has to.
  - Change comes back in gold, silver, and copper, like loot.
  - Electrum and platinum the purse doesn't need to spend are left alone.
  - Example: paying 2 sp from a purse of 1 gp leaves 8 sp.
- **`sell_price(item)`:** half the cost, rounded down to the copper. A 1 cp item sells for nothing and isn't offered.
- **`buy` and `sell`** (repository plus rules, each in one transaction):
  - `buy(repo, character, item, quantity)` checks the price, pays, and adds the items.
  - `sell(repo, character, item, quantity)` removes the items and pays out the sale price.
  - Equipped items can't be sold; the message says to unequip them in the Inventory first.
  - Both return a line for the shop's message, such as "Bought 2 × Torch for 2 cp."
- **`raise_dead(repo, character, payer)`:**
  - Charges the payer the temple's price.
  - Clears the dead flag and the death saves, and sets HP to 1.
  - Applies the −4 penalty if question 3 says so.

## UI

**A Shop dialog,** opened from a **Shop…** button in the main window's character row. It works for the selected character.

- **Header:** the character's name and purse.
- **Buy tab:**
  - Every item in a table with category, price, and weight, filterable by category.
  - A quantity box and a **Buy** button that shows the total.
  - Buy is disabled when the character can't afford the total, with the shortfall in a tooltip.
  - Proficiency and Strength warnings appear beside the button.
- **Sell tab:**
  - The character's items, each with its sale price.
  - A quantity box, and a **Sell** button. Equipped items are listed but marked, and Sell explains they must be unequipped first.
- **Temple tab:**
  - The player's dead characters, the price of *raise dead*, and a **Raise Dead** button.
  - It shows who pays (question 2).
  - The tab says "No one needs raising" when no one is dead.

The Inventory dialog's **Add** button changes according to question 1.

## Build phases

1. **Rules:**
   - `dnd/shop` with `pay`, `sell_price`, `buy`, `sell`, and `raise_dead`.
   - The penalty, if chosen: migration 22 adds the column, and combat applies it to attacks, saves, and checks.
   - Tests.
2. **UI:**
   - The Shop dialog, the main-window button, and the Inventory change.
   - The offscreen UI test.
   - README and doc updates.
   - The release tag. It also ships the Qt 6.4 combat-screen fix from `b69cf13`.

## Existing characters

Clean starts are fine. If migration 22 is needed, it defaults the penalty to 0, so nothing else changes.

## Decisions

Settled 2026-10-02. Every recommendation was taken except question 5:

1. **The Inventory keeps its free Add button,** renamed **Add (free)**. The Shop is the way to buy.
2. **Any character of the same player can pay for raising the dead,** the dead one included.
3. **The −4 *raise dead* penalty is implemented,** easing by 1 per long rest.
4. **Raising the dead costs 550 gp:** the 500 gp diamond plus a 50 gp fee.
5. **Equipped gear can't be sold** until it's unequipped in the Inventory.
6. **Give coins…** moves coins between the same player's characters.

## Open questions (settled)

1. **The Inventory's free Add button:**
   - **(a) Recommended:** keep it, renamed **Add (free)**, for giving out found loot or DM gifts. The Shop is the way to buy.
   - **(b)** Remove it. Items come only from shops, starting kits, and adventure loot.
2. **Who pays for raising the dead:**
   - **(a) Recommended:** pick any character of the same player, the dead one included (their purse survives them).
   - **(b)** Only the dead character's own purse.
   - **(c)** Pool the whole player's coins automatically.
3. **The *raise dead* penalty** (−4 to attack rolls, saving throws, and ability checks, reduced by 1 per long rest):
   - **(a) Recommended:** implement it. This needs one saved counter (migration 22), the long rest reducing it, and combat applying it. Initiative counts as a DEX check. The sheet shows it.
   - **(b)** Skip it, and note the gap in the README.
4. **The temple's price:**
   - **(a) Recommended:** 550 gp, which is the 500 gp diamond the spell consumes plus a 50 gp fee (the top of the PHB's 10–50 gp range for spellcasting services).
   - **(b)** 500 gp, the diamond alone.
   - **(c)** Something cheaper. Level-1 characters start with 5–25 gp, so raising the dead is out of reach until a few adventures in.
5. **Selling equipped gear:**
   - **(a) Recommended:** allowed. The shop asks for confirmation and unequips the item, so AC and attacks update.
   - **(b)** Not allowed until it's unequipped in the Inventory.
6. **Moving coins between characters:**
   - **(a) Recommended:** a **Give coins…** action in the Shop's header, between characters of the same player. This lets a party pool money for a raising, or outfit a new character.
   - **(b)** Not for now.
