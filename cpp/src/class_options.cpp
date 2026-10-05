#include "dnd/class_options.hpp"

#include <algorithm>

#include "dnd/class_features.hpp"

namespace dnd {

namespace {

constexpr bool kAuto = true;
constexpr bool kShown = false;

const char* const kMetamagicKey = "Metamagic";
const char* const kInvocationsKey = "Invocations";
const char* const kPactKey = "PactBoon";
const char* const kTomeKey = "TomeCantrips";
const char* const kExpertiseKey = "Expertise";
const char* const kBonusSkillsKey = "BonusSkills";

bool contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

}  // namespace

// --- Warlock ---------------------------------------------------------------------

const std::vector<InvocationInfo>& invocations() {
    static const std::vector<InvocationInfo> table = {
        {"Agonizing Blast", 2, "Eldritch Blast", "Add your CHA modifier to each Eldritch Blast beam's damage.", kAuto},
        {"Armor of Shadows", 2, "", "Mage Armor on yourself at will: AC 13 + DEX while unarmored.", kAuto},
        {"Beast Speech", 2, "", "Cast Speak with Animals at will.", kShown},
        {"Beguiling Influence", 2, "", "Proficiency in Deception and Persuasion.", kShown},
        {"Book of Ancient Secrets", 2, "Pact of the Tome", "Inscribe and cast rituals from any class.", kShown},
        {"Devil's Sight", 2, "", "See normally in darkness, magical or not, to 120 feet.", kShown},
        {"Eldritch Sight", 2, "", "Cast Detect Magic at will.", kShown},
        {"Eldritch Spear", 2, "Eldritch Blast", "Eldritch Blast's range becomes 300 feet.", kShown},
        {"Eyes of the Rune Keeper", 2, "", "Read all writing.", kShown},
        {"Fiendish Vigor", 2, "", "Cast False Life on yourself at will.", kShown},
        {"Gaze of Two Minds", 2, "", "See through a willing humanoid's senses.", kShown},
        {"Mask of Many Faces", 2, "", "Cast Disguise Self at will.", kShown},
        {"Misty Visions", 2, "", "Cast Silent Image at will.", kShown},
        {"Repelling Blast", 2, "Eldritch Blast", "Eldritch Blast pushes a creature 10 feet away. (Shown only: no map.)",
         kShown},
        {"Thief of Five Fates", 2, "", "Cast Bane once per long rest with a warlock slot.", kShown},
        {"Voice of the Chain Master", 2, "Pact of the Chain", "Speak and see through your familiar.", kShown},
        {"Mire the Mind", 5, "", "Cast Slow once per long rest with a warlock slot.", kShown},
        {"One with Shadows", 5, "", "Become invisible in dim light or darkness while still.", kShown},
        {"Sign of Ill Omen", 5, "", "Cast Bestow Curse once per long rest with a warlock slot.", kShown},
        {"Thirsting Blade", 5, "Pact of the Blade", "Attack twice with your pact weapon when you take the Attack action.",
         kAuto},
        {"Bewitching Whispers", 7, "", "Cast Compulsion once per long rest.", kShown},
        {"Dreadful Word", 7, "", "Cast Confusion once per long rest.", kShown},
        {"Sculptor of Flesh", 7, "", "Cast Polymorph once per long rest.", kShown},
        {"Ascendant Step", 9, "", "Cast Levitate on yourself at will.", kShown},
        {"Minions of Chaos", 9, "", "Cast Conjure Elemental once per long rest.", kShown},
        {"Otherworldly Leap", 9, "", "Cast Jump on yourself at will.", kShown},
        {"Whispers of the Grave", 9, "", "Cast Speak with Dead at will.", kShown},
        {"Lifedrinker", 12, "Pact of the Blade", "Your pact weapon deals extra necrotic damage equal to your CHA modifier.",
         kShown},
        {"Chains of Carceri", 15, "Pact of the Chain", "Cast Hold Monster at will on a celestial, fiend, or elemental.",
         kShown},
        {"Master of Myriad Forms", 15, "", "Cast Alter Self at will.", kShown},
        {"Visions of Distant Realms", 15, "", "Cast Arcane Eye at will.", kShown},
        {"Witch Sight", 15, "", "See the true form of shapechangers and illusions within 30 feet.", kShown},
    };
    return table;
}

const InvocationInfo* find_invocation(const std::string& name) {
    for (const auto& info : invocations()) {
        if (name == info.name) return &info;
    }
    return nullptr;
}

int invocations_known(int warlock_level) {
    if (warlock_level >= 18) return 8;
    if (warlock_level >= 15) return 7;
    if (warlock_level >= 12) return 6;
    if (warlock_level >= 9) return 5;
    if (warlock_level >= 7) return 4;
    if (warlock_level >= 5) return 3;
    if (warlock_level >= 2) return 2;
    return 0;
}

const char* pact_boon_description(const std::string& pact) {
    if (pact == "Pact of the Blade") {
        return "Your weapon is your pact weapon: you're proficient with the melee weapon you wield, and it counts as "
               "magical. (Summoning and bonding a weapon are taken as given.)";
    }
    if (pact == "Pact of the Chain") {
        return "A familiar in a special form (imp, pseudodragon, quasit, or sprite). (Shown only: familiars aren't "
               "simulated.)";
    }
    if (pact == "Pact of the Tome") {
        return "A Book of Shadows with three cantrips from any class's spell list, always prepared.";
    }
    return "";
}

bool has_armor_of_shadows(const Character& character) {
    return has_class_option(character, kInvocationsKey, "Armor of Shadows");
}

bool has_pact_blade(const Character& character) {
    return has_class_option(character, kPactKey, "Pact of the Blade");
}

bool has_agonizing_blast(const Character& character) {
    return has_class_option(character, kInvocationsKey, "Agonizing Blast");
}

bool has_thirsting_blade(const Character& character) {
    return has_pact_blade(character) && has_class_option(character, kInvocationsKey, "Thirsting Blade");
}

// --- Expertise ---------------------------------------------------------------------

int expertise_count(const std::string& class_name, int level) {
    if (class_name == "Rogue") return level >= 6 ? 4 : 2;
    if (class_name == "Bard") return level >= 10 ? 4 : level >= 3 ? 2 : 0;
    return 0;
}

bool has_expertise(const Character& character, Skill skill) {
    return has_class_option(character, kExpertiseKey, skill_name(skill));
}

// --- What's due ------------------------------------------------------------------

std::vector<ClassChoice> class_choices_due(const Character& character, const std::string& class_name,
                                           const SpellContext& spells) {
    const auto& cantrips = spells.cantrips;
    std::vector<ClassChoice> due;
    auto add = [&](const char* key, const char* label, int wanted, std::vector<std::string> options,
                   std::vector<std::string> descriptions) {
        int have = static_cast<int>(class_option(character, key).size());
        if (wanted > have && !options.empty()) {
            due.push_back({key, label, std::min(wanted - have, static_cast<int>(options.size())), std::move(options),
                           std::move(descriptions)});
        }
    };
    int level = character.level;

    if (class_name == "Sorcerer") {
        std::vector<std::string> options;
        std::vector<std::string> descriptions;
        for (Metamagic metamagic : all_metamagic()) {
            std::string name = metamagic_name(metamagic);
            if (has_class_option(character, kMetamagicKey, name)) continue;
            options.push_back(name);
            descriptions.push_back(metamagic_description(metamagic));
        }
        add(kMetamagicKey, "Metamagic", metamagic_known(level), options, descriptions);
    }
    if (class_name == "Warlock") {
        if (level >= 3) {
            std::vector<std::string> pacts(std::begin(kPactBoons), std::end(kPactBoons));
            std::vector<std::string> descriptions;
            for (const auto& pact : pacts) descriptions.push_back(pact_boon_description(pact));
            add(kPactKey, "Pact Boon", 1, pacts, descriptions);
        }
        std::vector<std::string> options;
        std::vector<std::string> descriptions;
        bool knows_blast = contains(spells.known, "Eldritch Blast");
        for (const auto& info : invocations()) {
            if (info.min_level > level || has_class_option(character, kInvocationsKey, info.name)) continue;
            std::string prerequisite = info.prerequisite;
            if (prerequisite.rfind("Pact of", 0) == 0 && !has_class_option(character, kPactKey, prerequisite)) continue;
            if (prerequisite == "Eldritch Blast" && !knows_blast) continue;
            options.push_back(info.name);
            descriptions.push_back(std::string(info.description) + (info.automated ? "" : " (Not automated yet.)"));
        }
        add(kInvocationsKey, "Eldritch Invocations", invocations_known(level), options, descriptions);
        if (has_class_option(character, kPactKey, "Pact of the Tome") && !cantrips.empty()) {
            std::vector<std::string> picks;
            for (const auto& cantrip : cantrips) {
                if (!has_class_option(character, kTomeKey, cantrip)) picks.push_back(cantrip);
            }
            add(kTomeKey, "Pact of the Tome cantrips", 3, picks, std::vector<std::string>(picks.size(), ""));
        }
    }
    if (int wanted = expertise_count(class_name, level); wanted > 0) {
        std::vector<std::string> options;
        for (int i = 0; i < kSkillCount; ++i) {
            auto skill = static_cast<Skill>(i);
            if (has_skill_proficiency(character, skill) && !has_expertise(character, skill)) {
                options.push_back(skill_name(skill));
            }
        }
        add(kExpertiseKey, "Expertise (double proficiency)", wanted, options,
            std::vector<std::string>(options.size(), "Double your proficiency bonus on this skill."));
    }
    if (character.subclass == "College of Lore" && level >= 3) {
        std::vector<std::string> options;
        for (int i = 0; i < kSkillCount; ++i) {
            auto skill = static_cast<Skill>(i);
            if (!has_skill_proficiency(character, skill)) options.push_back(skill_name(skill));
        }
        add(kBonusSkillsKey, "Bonus Proficiencies (College of Lore)", 3, options,
            std::vector<std::string>(options.size(), "Become proficient in this skill."));
    }
    return due;
}

std::string apply_class_choice(Character& character, const std::string& class_name, const std::string& key,
                               const std::vector<std::string>& picks, const SpellContext& spells) {
    auto due = class_choices_due(character, class_name, spells);
    auto choice = std::find_if(due.begin(), due.end(), [&](const ClassChoice& c) { return c.key == key; });
    if (choice == due.end()) return "Nothing to choose there right now.";
    if (static_cast<int>(picks.size()) != choice->count) {
        return "Choose " + std::to_string(choice->count) + " for " + choice->label + ".";
    }
    for (size_t i = 0; i < picks.size(); ++i) {
        if (!contains(choice->options, picks[i])) return picks[i] + " can't be chosen for " + choice->label + ".";
        for (size_t j = i + 1; j < picks.size(); ++j) {
            if (picks[i] == picks[j]) return picks[i] + " is chosen twice.";
        }
    }
    auto& stored = character.class_options[key];
    stored.insert(stored.end(), picks.begin(), picks.end());
    if (key == kBonusSkillsKey) {
        for (const auto& pick : picks) {
            for (int i = 0; i < kSkillCount; ++i) {
                if (pick == skill_name(static_cast<Skill>(i))) character.skill_proficiencies |= 1u << i;
            }
        }
    }
    return "";
}

}  // namespace dnd
