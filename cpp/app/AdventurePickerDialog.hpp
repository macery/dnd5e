#pragma once

#include <QDialog>
#include <vector>

#include "dnd/adventure_run.hpp"
#include "dnd/encounter_difficulty.hpp"
#include "dnd/monster.hpp"

class QLabel;
class QListWidget;

// Picks a solo adventure (docs/adventures-and-currency.md): adventures in
// progress to resume first, then every adventure with its fights, suggested
// level, and difficulty for the party (scaled toward Medium-Hard per fight);
// the details pane shows the hook, each fight's monsters and rating, camps,
// and rewards.
class AdventurePickerDialog : public QDialog {
    Q_OBJECT

public:
    // `party_levels`: who the ratings are for (the current player's living
    // characters by default). `in_progress`: runs that can be resumed.
    AdventurePickerDialog(std::vector<int> party_levels, std::vector<dnd::Monster> monsters,
                          std::vector<dnd::AdventureRun> in_progress, QWidget* parent = nullptr);

    // After Accepted: either a run to resume, or a new adventure.
    bool isResume() const;
    const dnd::AdventureRun& selectedRun() const;        // when isResume()
    const dnd::AdventureTemplate& selectedAdventure() const;  // when !isResume()

private slots:
    void onSelectionChanged(int index);

private:
    std::vector<dnd::AdventureRun> runs_;
    std::vector<dnd::ScaledAdventure> scaled_;  // parallel to dnd::adventure_templates()
    QListWidget* list_ = nullptr;
    QLabel* detailsLabel_ = nullptr;
};
