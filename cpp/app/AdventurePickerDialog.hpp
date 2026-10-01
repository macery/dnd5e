#pragma once

#include <QDialog>
#include <vector>

#include "dnd/encounter_difficulty.hpp"
#include "dnd/monster.hpp"

class QLabel;
class QListWidget;

// Lets the player pick one of the pre-built dnd::AdventureTemplate entries
// for a guided solo encounter: titles with their difficulty for the party on
// the left; the hook, the monsters (scaled toward Medium-Hard for the party,
// docs/monsters-and-encounters.md), and the reward on the right.
class AdventurePickerDialog : public QDialog {
    Q_OBJECT

public:
    // `party_levels`: the levels of the characters who'll fight.
    AdventurePickerDialog(std::vector<int> party_levels, std::vector<dnd::Monster> monsters,
                          QWidget* parent = nullptr);

    // The chosen adventure, scaled for the party. Only meaningful when
    // exec() returned QDialog::Accepted.
    const dnd::ScaledAdventure& selectedAdventure() const;

private slots:
    void onSelectionChanged(int index);

private:
    std::vector<dnd::ScaledAdventure> scaled_;
    QListWidget* list_ = nullptr;
    QLabel* detailsLabel_ = nullptr;
};
