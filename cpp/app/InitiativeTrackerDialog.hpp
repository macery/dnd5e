#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"

class QPushButton;
class QTableWidget;

// A transient, unpersisted view for running a combat encounter: rolls
// initiative (d20 + Dex modifier) for every character in the database and
// sorts by result, highest first. Not tied to a single player -- combat
// spans the whole party.
class InitiativeTrackerDialog : public QDialog {
    Q_OBJECT

public:
    explicit InitiativeTrackerDialog(std::vector<dnd::Character> characters, QWidget* parent = nullptr);

private slots:
    void onRollAllClicked();

private:
    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;
    std::vector<dnd::Character> characters_;

    QTableWidget* table_ = nullptr;
    QPushButton* rollAllButton_ = nullptr;
};
