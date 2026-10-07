#pragma once

#include <QDialog>
#include <vector>

#include "dnd/npc.hpp"
#include "dnd/repository.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

// The active campaign's NPCs (docs/scenes-and-npcs.md): the Shop's
// merchant, the Temple's priest, the people met in adventures, and any made
// here. Edit who they are, how they feel about the party, what they want
// and fear, and the stat block they'd fight with.
class NpcDialog : public QDialog {
    Q_OBJECT

public:
    explicit NpcDialog(dnd::Repository& repository, QWidget* parent = nullptr);

private:
    void reloadNpcs(int select_id);
    void showNpc();
    void newNpc();
    void saveNpc();
    void deleteNpc();
    int selectedId() const;

    dnd::Repository& repository_;
    int campaign_id_ = 0;
    std::vector<dnd::Npc> npcs_;
    QListWidget* npcList_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* roleEdit_ = nullptr;
    QComboBox* attitudeCombo_ = nullptr;
    QPlainTextEdit* descriptionEdit_ = nullptr;
    QLineEdit* wantsEdit_ = nullptr;
    QLineEdit* fearsEdit_ = nullptr;
    QCheckBox* knownBox_ = nullptr;
    QCheckBox* deadBox_ = nullptr;
    QComboBox* statBlockCombo_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QLabel* messageLabel_ = nullptr;
};
