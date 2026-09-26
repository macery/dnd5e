#pragma once

#include <string>

namespace dnd {

// A sample-data stat block, seeded read-only content -- foundation for a
// future battle engine. Deliberately excludes skills/saves/proficiency
// (5e monsters' bonuses are hand-picked exceptions, not a PC-style formula),
// attacks/damage (real design work for the battle engine itself), and
// flavor fields nothing in the app uses yet (speed, size, type).
struct Monster {
    int monster_id = 0;
    std::string name;
    int armor_class = 10;
    int hit_points = 1;
    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;
    double challenge_rating = 0.0;
};

}  // namespace dnd
