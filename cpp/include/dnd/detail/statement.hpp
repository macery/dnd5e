#pragma once

// Internal implementation detail, shared by repository.cpp and migrations.cpp.
// Not part of dndcore's stable API.

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace dnd::detail {

// Thin RAII wrapper so every prepared statement gets finalized, even if a
// bind or step call throws.
class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("failed to prepare statement: ") + sqlite3_errmsg(db));
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    sqlite3* db() const { return db_; }

private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

inline void bind_text(Statement& stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt.get(), index, value.c_str(), -1, SQLITE_TRANSIENT);
}

inline void run_and_expect_done(Statement& stmt, const char* what) {
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error(std::string("failed to run ") + what + ": " + sqlite3_errmsg(stmt.db()));
    }
}

}  // namespace dnd::detail
