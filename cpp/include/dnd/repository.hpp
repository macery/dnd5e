#pragma once

#include <functional>
#include <string>
#include <vector>

#include "dnd/adventure_run.hpp"
#include "dnd/character.hpp"
#include "dnd/item.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/monster.hpp"
#include "dnd/npc.hpp"
#include "dnd/player.hpp"
#include "dnd/spell.hpp"

struct sqlite3;

namespace dnd {

// Owns a SQLite connection at `db_path`, running any pending schema
// migrations (see dnd/migrations.hpp) on construction. Pass ":memory:" for
// an ephemeral in-memory database (used by tests).
class Repository {
public:
    explicit Repository(const std::string& db_path);
    ~Repository();

    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    Repository(Repository&& other) noexcept;
    Repository& operator=(Repository&& other) noexcept;

    std::vector<Player> get_players() const;
    std::vector<LookupEntry> get_classes() const;
    std::vector<LookupEntry> get_races() const;
    std::vector<LookupEntry> get_alignments() const;
    std::vector<Character> get_characters() const;
    std::vector<Character> get_characters_by_player(int player_id) const;
    std::vector<Monster> get_monsters() const;
    std::vector<Item> get_items() const;

    // The character's item stacks, ordered by item name. Deleting a
    // character (or its player) removes its inventory too.
    std::vector<InventoryEntry> get_inventory(int character_id) const;
    // Adds `quantity` (> 0) of the item, stacking onto any existing entry.
    void add_to_inventory(int character_id, int item_id, int quantity = 1);
    // Removes up to `quantity` (> 0) of the item, deleting the entry once
    // none are left. Returns how many were actually removed (0 if the
    // character had none).
    int remove_from_inventory(int character_id, int item_id, int quantity = 1);
    // Spends one use of a limited-use item (Item::uses > 0). Its last use
    // removes that item. An item without tracked uses is removed outright.
    void spend_item_use(int character_id, int item_id);
    // Marks an inventory entry worn/wielded or not. Which combinations are
    // allowed (one armor, one shield, ...) is dnd::equip's job (dnd/equipment.hpp).
    void set_equipped(int character_id, int item_id, bool equipped);
    // Marks an inventory entry attuned or not. The rules (one per short rest,
    // at most three) are dnd/magic_items.hpp's.
    void set_attuned(int character_id, int item_id, bool attuned);

    // Every seeded spell, ordered by level then name, with class_names
    // filled from ClassSpells.
    std::vector<Spell> get_spells() const;
    // The character's known (castable) spells, same order and detail.
    std::vector<Spell> get_character_spells(int character_id) const;
    // Adding a spell the character already knows is a no-op. Spells are
    // deleted with their character.
    void add_character_spell(int character_id, int spell_id, bool prepared = true);
    // A Wizard's spellbook spell, prepared or not (docs/preparing-spells.md).
    void set_spell_prepared(int character_id, int spell_id, bool prepared);
    void remove_character_spell(int character_id, int spell_id);

    int add_player(const std::string& player_name);
    int add_character(const Character& character);

    void update_player(const Player& player);
    // Deletes the player and, to satisfy the PlayerID foreign key (enforced
    // via PRAGMA foreign_keys), all of that player's characters too, in one
    // transaction.
    void delete_player(int player_id);

    void update_character(const Character& character);
    void delete_character(int character_id);

    // Adventure progress (dnd/adventure_run.hpp). add returns the new run_id.
    int add_adventure_run(const AdventureRun& run);
    void update_adventure_run(const AdventureRun& run);
    // Newest first; with `in_progress_only`, just the ones that can be resumed.
    std::vector<AdventureRun> get_adventure_runs(bool in_progress_only = false) const;

    // Campaigns (docs/campaigns.md), by name. Deleting one keeps its
    // characters and runs; they just leave it.
    int add_campaign(const Campaign& campaign);
    void update_campaign(const Campaign& campaign);  // name and notes
    void delete_campaign(int campaign_id);
    std::vector<Campaign> get_campaigns() const;
    // The active campaign's id, or 0 when none is.
    int active_campaign_id() const;
    // Makes `campaign_id` the only active campaign (0: none).
    void set_active_campaign(int campaign_id);
    // Moves a character into a campaign (0: out of any).
    void set_character_campaign(int character_id, int campaign_id);

    // NPCs (dnd/npc.hpp). Deleting a campaign deletes its NPCs.
    int add_npc(const Npc& npc);
    void update_npc(const Npc& npc);
    void delete_npc(int npc_id);
    // One campaign's NPCs (0: those with no campaign), ordered by name.
    std::vector<Npc> get_npcs(int campaign_id) const;

    // Runs `work` atomically: everything it changes is committed, or -- if it
    // throws -- rolled back and the exception rethrown. Nests safely (a
    // savepoint, not BEGIN).
    void with_transaction(const std::function<void()>& work);

    // Writes a complete copy of the database to `path` (SQLite's online
    // backup), overwriting it. Throws std::runtime_error on failure.
    void backup_to(const std::string& path) const;

private:
    sqlite3* db_ = nullptr;
};

}  // namespace dnd
