#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_class.hpp"
#include "dnd/player.hpp"

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
    std::vector<CharacterClass> get_classes() const;
    std::vector<Character> get_characters() const;
    std::vector<Character> get_characters_by_player(int player_id) const;

    int add_player(const std::string& player_name);
    int add_character(const Character& character);

private:
    sqlite3* db_ = nullptr;
};

}  // namespace dnd
