#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/item.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

// A character's carried items: add from the seeded item list, drop, give to
// another character, equip
// weapons, armor, and shields (which set AC and the wielded weapon), and use
// healing consumables and scrolls outside combat (in combat that's Drink
// Potion and Read Scroll). Every
// change is persisted immediately via Repository.
class InventoryDialog : public QDialog {
    Q_OBJECT

public:
    InventoryDialog(dnd::Repository& repository, dnd::Character character,
                    const std::vector<dnd::LookupEntry>& classes, const std::vector<dnd::LookupEntry>& races,
                    QWidget* parent = nullptr);

private slots:
    void onAddClicked();
    void onDropClicked();
    void onUseClicked();
    void onEquipClicked();
    void onGiveClicked();
    void onSelectionChanged();

private:
    void reload();
    // Reads a healing scroll outside combat: asks who it's for, then reports the result.
    void readScroll(const dnd::Item& scroll);
    // The inventory entry for the selected table row, or nullptr.
    const dnd::InventoryEntry* selectedEntry() const;

    dnd::Repository& repository_;
    dnd::Character character_;
    std::string className_;
    std::string raceName_;
    std::vector<dnd::InventoryEntry> inventory_;

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_{randomSource_};

    QLabel* hpLabel_ = nullptr;
    QLabel* weightLabel_ = nullptr;
    QLabel* coinsLabel_ = nullptr;
    QLabel* equipmentLabel_ = nullptr;
    QPushButton* equipButton_ = nullptr;
    QTableWidget* table_ = nullptr;
    QComboBox* itemCombo_ = nullptr;
    QSpinBox* quantitySpin_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* dropButton_ = nullptr;
    QPushButton* giveButton_ = nullptr;
    QPushButton* useButton_ = nullptr;
    QLabel* messageLabel_ = nullptr;
};
