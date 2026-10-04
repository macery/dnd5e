#include "dnd/subclass.hpp"

#include <algorithm>
#include <stdexcept>

namespace dnd {

namespace {

constexpr bool kAuto = true;
constexpr bool kShown = false;

}  // namespace

const std::vector<SubclassInfo>& subclasses() {
    static const std::vector<SubclassInfo> table = {
        {"Path of the Berserker", "Barbarian",
         "Rage with abandon: Frenzy adds a bonus-action attack each turn, at the cost of exhaustion.",
         {{3, "Frenzy", kAuto}, {6, "Mindless Rage", kShown}, {10, "Intimidating Presence", kShown},
          {14, "Retaliation", kShown}},
         {}},
        {"College of Lore", "Bard",
         "Learning and wit: Cutting Words turns an enemy's hit into a miss.",
         {{3, "Bonus Proficiencies (three skills: set them in Edit Character)", kShown},
          {3, "Cutting Words", kAuto}, {6, "Additional Magical Secrets", kShown}, {14, "Peerless Skill", kShown}},
         {}},
        {"Life Domain", "Cleric",
         "The healer: stronger healing spells, and Channel Divinity to restore the whole party.",
         {{1, "Bonus Proficiency (heavy armor)", kAuto}, {1, "Disciple of Life", kAuto},
          {1, "Domain spells (always prepared)", kAuto}, {2, "Channel Divinity: Preserve Life", kAuto},
          {6, "Blessed Healer", kShown}, {8, "Divine Strike", kShown}, {17, "Supreme Healing", kShown}},
         {{1, "Bless"}, {1, "Cure Wounds"}, {3, "Lesser Restoration"}, {3, "Spiritual Weapon"},
          {5, "Beacon of Hope"}, {5, "Revivify"}}},
        {"Circle of the Land", "Druid",
         "Keepers of old lore: an extra cantrip, slots back on a short rest, and spells of their land.",
         {{2, "Bonus Cantrip", kAuto}, {2, "Natural Recovery", kAuto}, {3, "Circle Spells (always prepared)", kAuto},
          {6, "Land's Stride", kShown}, {10, "Nature's Ward", kShown}, {14, "Nature's Sanctuary", kShown}},
         {{3, "Hold Person", "Arctic"}, {3, "Spike Growth", "Arctic"}, {5, "Sleet Storm", "Arctic"},
          {5, "Slow", "Arctic"},
          {3, "Mirror Image", "Coast"}, {3, "Misty Step", "Coast"}, {5, "Water Breathing", "Coast"},
          {5, "Water Walk", "Coast"},
          {3, "Blur", "Desert"}, {3, "Silence", "Desert"}, {5, "Create Food and Water", "Desert"},
          {5, "Protection from Energy", "Desert"},
          {3, "Barkskin", "Forest"}, {3, "Spider Climb", "Forest"}, {5, "Call Lightning", "Forest"},
          {5, "Plant Growth", "Forest"},
          {3, "Invisibility", "Grassland"}, {3, "Pass without Trace", "Grassland"}, {5, "Daylight", "Grassland"},
          {5, "Haste", "Grassland"},
          {3, "Spider Climb", "Mountain"}, {3, "Spike Growth", "Mountain"}, {5, "Lightning Bolt", "Mountain"},
          {5, "Meld into Stone", "Mountain"},
          {3, "Darkness", "Swamp"}, {3, "Acid Arrow", "Swamp"}, {5, "Water Walk", "Swamp"},
          {5, "Stinking Cloud", "Swamp"},
          {3, "Spider Climb", "Underdark"}, {3, "Web", "Underdark"}, {5, "Gaseous Form", "Underdark"},
          {5, "Stinking Cloud", "Underdark"}},
         SubclassSpellMode::AlwaysPrepared, "Land",
         {"Arctic", "Coast", "Desert", "Forest", "Grassland", "Mountain", "Swamp", "Underdark"}},
        {"Champion", "Fighter",
         "Raw physical power: critical hits on a 19 or 20.",
         {{3, "Improved Critical", kAuto}, {7, "Remarkable Athlete", kShown}, {10, "Additional Fighting Style", kShown},
          {15, "Superior Critical", kShown}, {18, "Survivor", kShown}},
         {}},
        {"Way of the Open Hand", "Monk",
         "Masters of unarmed combat: Flurry of Blows knocks foes down.",
         {{3, "Open Hand Technique", kAuto}, {6, "Wholeness of Body", kShown}, {11, "Tranquility", kShown},
          {17, "Quivering Palm", kShown}},
         {}},
        {"Oath of Devotion", "Paladin",
         "Honesty, courage, compassion: Sacred Weapon and Turn the Unholy.",
         {{3, "Oath spells (always prepared)", kAuto}, {3, "Channel Divinity: Sacred Weapon", kAuto},
          {3, "Channel Divinity: Turn the Unholy", kAuto}, {7, "Aura of Devotion", kShown},
          {15, "Purity of Spirit", kShown}, {20, "Holy Nimbus", kShown}},
         {{3, "Protection from Evil and Good"}, {3, "Sanctuary"}, {5, "Lesser Restoration"}, {5, "Zone of Truth"}}},
        {"Hunter", "Ranger",
         "The monster slayer: Colossus Slayer finishes off the wounded.",
         {{3, "Hunter's Prey: Colossus Slayer", kAuto}, {7, "Defensive Tactics", kShown},
          {11, "Multiattack", kShown}, {15, "Superior Hunter's Defense", kShown}},
         {}},
        {"Thief", "Rogue",
         "Quick hands and quicker feet: Cunning Action can drink a potion.",
         {{3, "Fast Hands (drink a potion as a bonus action)", kAuto}, {3, "Second-Story Work", kShown},
          {9, "Supreme Sneak", kShown}, {13, "Use Magic Device", kShown}, {17, "Thief's Reflexes", kShown}},
         {}},
        {"Draconic Bloodline", "Sorcerer",
         "Dragon blood: tougher (AC 13 + DEX unarmored, +1 HP per level).",
         {{1, "Dragon Ancestor", kAuto}, {1, "Draconic Resilience", kAuto}, {6, "Elemental Affinity", kShown},
          {14, "Dragon Wings", kShown}, {18, "Draconic Presence", kShown}},
         {}, SubclassSpellMode::AlwaysPrepared, "Dragon ancestor",
         {"Black (acid)", "Blue (lightning)", "Brass (fire)", "Bronze (lightning)", "Copper (acid)", "Gold (fire)",
          "Green (poison)", "Red (fire)", "Silver (cold)", "White (cold)"}},
        {"The Fiend", "Warlock",
         "A pact with a fiend: temporary hit points for every kill, and fiery spells.",
         {{1, "Expanded Spell List", kAuto}, {1, "Dark One's Blessing", kAuto}, {6, "Dark One's Own Luck", kShown},
          {10, "Fiendish Resilience", kShown}, {14, "Hurl Through Hell", kShown}},
         {{1, "Burning Hands"}, {1, "Command"}, {3, "Blindness/Deafness"}, {3, "Scorching Ray"},
          {5, "Fireball"}, {5, "Stinking Cloud"}},
         SubclassSpellMode::Expanded},
        {"School of Evocation", "Wizard",
         "Elemental power. (Its early features need a map: area spells here already hit only the targets you "
         "choose.)",
         {{2, "Evocation Savant", kShown}, {2, "Sculpt Spells", kShown}, {6, "Potent Cantrip", kShown},
          {10, "Empowered Evocation", kShown}, {14, "Overchannel", kShown}},
         {}},
    };
    return table;
}

const SubclassInfo* find_subclass(const std::string& name) {
    for (const auto& info : subclasses()) {
        if (name == info.name) return &info;
    }
    return nullptr;
}

std::vector<const SubclassInfo*> subclasses_for(const std::string& class_name) {
    std::vector<const SubclassInfo*> found;
    for (const auto& info : subclasses()) {
        if (class_name == info.class_name) found.push_back(&info);
    }
    return found;
}

int subclass_level(const std::string& class_name) {
    if (class_name == "Cleric" || class_name == "Sorcerer" || class_name == "Warlock") return 1;
    if (class_name == "Druid" || class_name == "Wizard") return 2;
    if (subclasses_for(class_name).empty()) return 0;
    return 3;
}

std::string subclass_kind(const std::string& class_name) {
    static const std::vector<std::pair<std::string, std::string>> kinds = {
        {"Barbarian", "Primal Path"},  {"Bard", "Bard College"},          {"Cleric", "Divine Domain"},
        {"Druid", "Druid Circle"},     {"Fighter", "Martial Archetype"}, {"Monk", "Monastic Tradition"},
        {"Paladin", "Sacred Oath"},    {"Ranger", "Ranger Archetype"},   {"Rogue", "Roguish Archetype"},
        {"Sorcerer", "Sorcerous Origin"}, {"Warlock", "Otherworldly Patron"}, {"Wizard", "Arcane Tradition"}};
    for (const auto& [name, kind] : kinds) {
        if (name == class_name) return kind;
    }
    return "Subclass";
}

bool subclass_due(const Character& character, const std::string& class_name) {
    int level = subclass_level(class_name);
    return level > 0 && character.level >= level && character.subclass.empty();
}

bool has_subclass(const Character& character, const char* subclass, int min_level) {
    return character.subclass == subclass && character.level >= min_level;
}

std::vector<ClassFeatureName> subclass_features_at(const std::string& subclass, int level) {
    std::vector<ClassFeatureName> found;
    if (const SubclassInfo* info = find_subclass(subclass)) {
        for (const auto& feature : info->features) {
            if (feature.level == level) found.push_back(feature);
        }
    }
    return found;
}

std::vector<std::string> subclass_spell_names(const Character& character) {
    std::vector<std::string> names;
    const SubclassInfo* info = find_subclass(character.subclass);
    if (!info) return names;
    for (const auto& spell : info->spells) {
        if (spell.class_level > character.level) continue;
        if (*spell.choice && character.subclass_choice != spell.choice) continue;
        if (std::find(names.begin(), names.end(), spell.spell) == names.end()) names.push_back(spell.spell);
    }
    return names;
}

std::vector<std::string> always_prepared_spell_names(const Character& character) {
    const SubclassInfo* info = find_subclass(character.subclass);
    if (!info || info->spell_mode != SubclassSpellMode::AlwaysPrepared) return {};
    return subclass_spell_names(character);
}

bool on_spell_list(const Spell& spell, const std::string& class_name, const Character& character) {
    if (std::find(spell.class_names.begin(), spell.class_names.end(), class_name) != spell.class_names.end()) {
        return true;
    }
    const SubclassInfo* info = find_subclass(character.subclass);
    if (!info || info->spell_mode != SubclassSpellMode::Expanded) return false;
    // A patron's expanded list: any of its spells, at any level the warlock can cast.
    return std::any_of(info->spells.begin(), info->spells.end(),
                       [&](const SubclassSpell& s) { return spell.name == s.spell; });
}

std::string subclass_refusal(const Character& character, const std::string& class_name, const std::string& subclass,
                             const std::string& choice) {
    const SubclassInfo* info = find_subclass(subclass);
    if (!info || class_name != info->class_name) return subclass + " isn't a " + class_name + " subclass.";
    if (!character.subclass.empty()) return character.character_name + " already follows " + character.subclass + ".";
    int level = subclass_level(class_name);
    if (character.level < level) {
        return class_name + "s choose a " + subclass_kind(class_name) + " at level " + std::to_string(level) + ".";
    }
    if (!info->choices.empty() &&
        std::find(info->choices.begin(), info->choices.end(), choice) == info->choices.end()) {
        return "Choose a " + std::string(info->choice_label) + ".";
    }
    return "";
}

void choose_subclass(Character& character, const std::string& class_name, const std::string& subclass,
                     const std::string& choice) {
    std::string refusal = subclass_refusal(character, class_name, subclass, choice);
    if (!refusal.empty()) throw std::invalid_argument(refusal);
    character.subclass = subclass;
    character.subclass_choice = find_subclass(subclass)->choices.empty() ? "" : choice;
    if (subclass == "Draconic Bloodline") {  // Draconic Resilience: +1 HP per sorcerer level, from level 1
        character.max_hp += character.level;
        character.current_hp += character.level;
    }
}

}  // namespace dnd
