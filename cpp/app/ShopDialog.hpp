#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/item.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

// Buying and selling gear for one character, a temple that raises the
// player's dead, and handing coins to another of the player's characters
// (docs/shops.md). Every change is saved at once through dnd/shop.
class ShopDialog : public QDialog {
    Q_OBJECT

public:
    ShopDialog(dnd::Repository& repository, dnd::Character character, const std::vector<dnd::LookupEntry>& classes,
               const std::vector<dnd::LookupEntry>& races, QWidget* parent = nullptr);

    // The tabs, for the main window (a dead character opens on the Temple).
    QTabWidget* tabs() const { return tabs_; }

private slots:
    void onBuyClicked();
    void onSellClicked();
    void onRaiseClicked();
    void onGiveClicked();
    void refreshBuyControls();
    void refreshSellControls();
    void refreshTemple();

private:
    void reload();
    void fillBuyTable();
    const dnd::Item* selectedShopItem() const;
    const dnd::InventoryEntry* selectedPackEntry() const;
    void showMessage(const std::string& message, bool ok);

    dnd::Repository& repository_;
    dnd::Character character_;
    std::string className_;
    std::string raceName_;
    std::vector<dnd::Item> items_;            // every item, in the shop's order
    std::vector<int> shownItems_;             // indexes into items_ for the Buy table's rows
    std::vector<dnd::InventoryEntry> pack_;   // the Sell table's rows
    std::vector<dnd::Character> party_;       // the player's characters, for the Temple

    QLabel* purseLabel_ = nullptr;
    QTabWidget* tabs_ = nullptr;

    QComboBox* categoryCombo_ = nullptr;
    QTableWidget* buyTable_ = nullptr;
    QSpinBox* buyQuantity_ = nullptr;
    QPushButton* buyButton_ = nullptr;
    QLabel* buyWarnings_ = nullptr;

    QTableWidget* sellTable_ = nullptr;
    QSpinBox* sellQuantity_ = nullptr;
    QPushButton* sellButton_ = nullptr;
    QLabel* sellHint_ = nullptr;

    QLabel* templeIntro_ = nullptr;
    QComboBox* deadCombo_ = nullptr;
    QComboBox* payerCombo_ = nullptr;
    QPushButton* raiseButton_ = nullptr;

    QLabel* messageLabel_ = nullptr;
};

// Moves coins from one character to another of the same player: who gets
// them, and how many of each coin (capped at what the giver has).
class GiveCoinsDialog : public QDialog {
    Q_OBJECT

public:
    GiveCoinsDialog(dnd::Repository& repository, dnd::Character giver, QWidget* parent = nullptr);

    // The line dnd::give_coins returned; empty until coins were given.
    const std::string& message() const { return message_; }

private:
    void give();

    dnd::Repository& repository_;
    dnd::Character giver_;
    QComboBox* recipientCombo_ = nullptr;
    std::vector<QSpinBox*> coinSpins_;  // cp, sp, ep, gp, pp
    QLabel* errorLabel_ = nullptr;
    std::string message_;
};
