#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/item.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

// A character's carried items: add from the seeded item list, drop, and use
// healing consumables (outside combat -- in combat that's the Drink Potion
// action). Every change is persisted immediately via Repository.
class InventoryDialog : public QDialog {
    Q_OBJECT

public:
    InventoryDialog(dnd::Repository& repository, dnd::Character character, QWidget* parent = nullptr);

private slots:
    void onAddClicked();
    void onDropClicked();
    void onUseClicked();
    void onSelectionChanged();

private:
    void reload();
    // The inventory entry for the selected table row, or nullptr.
    const dnd::InventoryEntry* selectedEntry() const;

    dnd::Repository& repository_;
    dnd::Character character_;
    std::vector<dnd::InventoryEntry> inventory_;

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_{randomSource_};

    QLabel* hpLabel_ = nullptr;
    QLabel* weightLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QComboBox* itemCombo_ = nullptr;
    QSpinBox* quantitySpin_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* dropButton_ = nullptr;
    QPushButton* useButton_ = nullptr;
    QLabel* messageLabel_ = nullptr;
};
