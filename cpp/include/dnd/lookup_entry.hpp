#pragma once

#include <string>

namespace dnd {

// A generic id+name row for read-only reference tables (Classes, Races,
// Alignments): seeded once via migrations, never written to by the UI.
struct LookupEntry {
    int id = 0;
    std::string name;
};

}  // namespace dnd
