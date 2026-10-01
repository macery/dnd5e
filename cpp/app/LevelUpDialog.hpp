#pragma once

#include <QWizard>
#include <memory>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

struct LevelUpContext;

// Takes a character up one level (docs/level-up.md): hit points (roll or
// average), an Ability Score Improvement and a Fighting Style when they're
// due, new spells, and a summary of what the level brings. Pages that don't
// apply are skipped. Finish applies the level with dnd::apply_level_up and
// saves the character and any new spells. With several levels pending, the
// caller runs it once per level.
class LevelUpDialog : public QWizard {
    Q_OBJECT

public:
    // `character` must have a pending level (dnd::pending_levels > 0) and a
    // known class.
    LevelUpDialog(dnd::Repository& repository, const dnd::Character& character,
                  const std::vector<dnd::LookupEntry>& classes, QWidget* parent = nullptr);
    ~LevelUpDialog() override;

    // The character after Finish (unchanged before).
    const dnd::Character& character() const;

    void accept() override;

private:
    dnd::Repository& repository_;
    std::unique_ptr<LevelUpContext> context_;
};
