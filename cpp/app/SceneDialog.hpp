#pragma once

#include <QDialog>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/adventure_run.hpp"
#include "dnd/dice.hpp"
#include "dnd/repository.hpp"
#include "dnd/scenes.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

// A scene that isn't a fight (docs/scenes-and-npcs.md): a challenge, a trap,
// or a choice. Pick an option, who attempts it, and who helps; roll; see
// what happened. Accepting (Continue) means the scene is done: its outcome
// is applied and the run moves on. Closing before that leaves the run at
// this scene for later.
class SceneDialog : public QDialog {
    Q_OBJECT

public:
    SceneDialog(dnd::Repository& repository, dnd::AdventureRun& run, const dnd::AdventureTemplate& adventure,
                const dnd::AdventureScene& scene, const QString& title, QWidget* parent = nullptr);

private:
    void refreshRoller();
    void attempt();
    void finish(const QStringList& lines);
    // The options as shown: the scene's own, or a trap's (disarm / go around).
    void buildTrapOptions();

    dnd::Repository& repository_;
    dnd::AdventureRun& run_;
    const dnd::AdventureTemplate& adventure_;
    const dnd::AdventureScene& scene_;
    std::vector<dnd::CheckRoller> party_;
    std::vector<dnd::SceneOption> options_;  // what the list shows

    dnd::SystemRandomSource random_;
    dnd::Dice dice_{random_};

    QListWidget* optionList_ = nullptr;
    QComboBox* rollerCombo_ = nullptr;
    QComboBox* helperCombo_ = nullptr;
    QCheckBox* inspirationBox_ = nullptr;
    QPushButton* attemptButton_ = nullptr;
    QPushButton* continueButton_ = nullptr;
    QLabel* resultLabel_ = nullptr;
    bool done_ = false;
};
