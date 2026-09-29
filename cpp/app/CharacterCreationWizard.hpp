#pragma once

#include <QWizard>
#include <memory>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/dice.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"
#include "dnd/spell.hpp"
#include "dnd/weapon.hpp"

// State shared by every page of the wizard: the choices built up so far plus
// the lookup data the pages display. Pages read and write `choices`; the
// rules (validation, skill plans, the final character) all come from
// dnd/character_creation.hpp, so nothing here re-implements 5e math.
struct CharacterCreationContext {
    dnd::CreationChoices choices;
    bool take_explorers_pack = true;
    std::vector<int> chosen_spell_ids;  // cantrips + levelled spells, casters only

    std::vector<dnd::LookupEntry> classes;
    std::vector<dnd::LookupEntry> races;
    std::vector<dnd::LookupEntry> alignments;
    std::vector<dnd::Weapon> weapons;
    std::vector<dnd::Spell> spells;  // every seeded spell
    std::vector<std::string> existing_names;  // CharacterName is UNIQUE

    dnd::SystemRandomSource randomSource;
    dnd::Dice dice{randomSource};
};

// Guided, step-by-step creation of a rules-valid level-1 character:
// Class -> Race -> Ability Scores -> Background -> Skills -> [Spells, for
// classes that cast at level 1] -> Details -> Equipment -> Review. On Finish
// it saves the character, their spells, and (if chosen) the Explorer's Pack
// into their inventory via Repository.
class CharacterCreationWizard : public QWizard {
    Q_OBJECT

public:
    CharacterCreationWizard(dnd::Repository& repository, int player_id, const std::vector<dnd::Weapon>& weapons,
                            QWidget* parent = nullptr);
    ~CharacterCreationWizard() override;

    void accept() override;

private:
    dnd::Repository& repository_;
    std::unique_ptr<CharacterCreationContext> context_;
};
