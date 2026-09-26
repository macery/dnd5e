#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/monster.hpp"
#include "dnd/player.hpp"
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
