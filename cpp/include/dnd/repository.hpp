#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/item.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/monster.hpp"
#include "dnd/player.hpp"
#include "dnd/spell.hpp"
#include "dnd/weapon.hpp"

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
    std::vector<Weapon> get_weapons() const;
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

    // Every seeded spell, ordered by level then name, with class_names
    // filled from ClassSpells.
    std::vector<Spell> get_spells() const;
    // The character's known (castable) spells, same order and detail.
    std::vector<Spell> get_character_spells(int character_id) const;
    // Adding a spell the character already knows is a no-op. Spells are
    // deleted with their character.
    void add_character_spell(int character_id, int spell_id);
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

private:
    sqlite3* db_ = nullptr;
};

}  // namespace dnd
