#pragma once

#include <climits>

struct sqlite3;

namespace dnd {

struct Migration {
    int version;
    const char* description;
    const char* sql;
};

// Reads the current schema version from (and bootstraps, if needed) the
// SchemaMigrations tracking table.
int current_schema_version(sqlite3* db);

// Applies, in order, every migration newer than the database's current
// version -- each inside its own transaction, rolled back on failure -- and
// records it in SchemaMigrations as it commits. Safe to call on every open:
// a fully up-to-date database is a no-op. `up_to_version` stops after that
// version (for tests that need a database as it was at some older version).
void run_migrations(sqlite3* db, int up_to_version = INT_MAX);

}  // namespace dnd
