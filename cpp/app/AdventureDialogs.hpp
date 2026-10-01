#pragma once

#include <QDialog>
#include <QStringList>
#include <vector>

#include "dnd/adventure_run.hpp"
#include "dnd/character.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/player.hpp"
#include "dnd/repository.hpp"

class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;

// Dialogs for multi-scene adventures (docs/adventures-and-currency.md).

// Choose who goes on an adventure: every living character, checked by
// default if they belong to `default_player_id`.
class PartyDialog : public QDialog {
    Q_OBJECT

public:
    PartyDialog(const std::vector<dnd::Character>& living, const std::vector<dnd::Player>& players,
                const std::vector<dnd::LookupEntry>& classes, int default_player_id, const QString& adventure,
                QWidget* parent = nullptr);
    std::vector<int> partyIds() const;  // only meaningful after Accepted

private:
    QListWidget* list_ = nullptr;
};

// A short rest (PHB ch. 8) for one or more characters: each chooses how many
// hit dice to spend; short-rest features, pact slots, and Arcane Recovery
// come back automatically. Rest applies it, saves, and shows what happened.
class ShortRestDialog : public QDialog {
    Q_OBJECT

public:
    ShortRestDialog(dnd::Repository& repository, std::vector<dnd::Character> characters,
                    const std::vector<dnd::LookupEntry>& classes, QWidget* parent = nullptr);
    bool rested() const { return rested_; }
    const QStringList& summary() const { return summary_; }

private:
    void rest();

    dnd::Repository& repository_;
    std::vector<dnd::Character> characters_;
    std::vector<dnd::LookupEntry> classes_;
    std::vector<QSpinBox*> spins_;
    QLabel* resultLabel_ = nullptr;
    QPushButton* restButton_ = nullptr;
    bool rested_ = false;
    QStringList summary_;
};

// Between two scenes of an adventure: the party's state, and the choices --
// press on, a short rest, a long rest (at a camp, then Level Up for anyone
// ready), or retreat. Closing the window stops for now (progress is saved).
class BetweenScenesDialog : public QDialog {
    Q_OBJECT

public:
    enum class Choice { PressOn, Retreat, StopForNow };

    BetweenScenesDialog(dnd::Repository& repository, dnd::AdventureRun& run, const dnd::AdventureTemplate& adventure,
                        const std::vector<dnd::LookupEntry>& classes, QWidget* parent = nullptr);
    Choice choice() const { return choice_; }

private:
    void refresh();
    std::vector<dnd::Character> party() const;  // living members, fresh from the database
    void onShortRest();
    void onLongRest();

    dnd::Repository& repository_;
    dnd::AdventureRun& run_;
    const dnd::AdventureTemplate& adventure_;
    std::vector<dnd::LookupEntry> classes_;
    Choice choice_ = Choice::StopForNow;
    QTableWidget* table_ = nullptr;
    QLabel* messageLabel_ = nullptr;
    QPushButton* longRestButton_ = nullptr;
};
