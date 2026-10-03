#include "ShopDialog.hpp"

#include <algorithm>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "WindowSizing.hpp"
#include "dnd/currency.hpp"
#include "dnd/equipment.hpp"
#include "dnd/inventory.hpp"
#include "dnd/shop.hpp"

namespace {

std::string nameFor(const std::vector<dnd::LookupEntry>& entries, int id) {
    for (const auto& entry : entries) {
        if (entry.id == id) return entry.name;
    }
    return "";
}

QString text(const std::string& s) {
    return QString::fromStdString(s);
}

QString cost(int cp) {
    return text(dnd::format_cost(cp));
}

QTableWidget* makeTable(const QStringList& headers, QWidget* parent) {
    auto* table = new QTableWidget(0, static_cast<int>(headers.size()), parent);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    return table;
}

// "Phi (Beth)": a character and their player, for choosing between
// characters of different players.
QString withPlayer(const dnd::Character& character, const std::vector<dnd::Player>& players) {
    for (const auto& player : players) {
        if (player.player_id == character.player_id) {
            return QString("%1 (%2)").arg(text(character.character_name), text(player.player_name));
        }
    }
    return text(character.character_name);
}

// Every living character except `giver`, labeled with their player.
void fillRecipients(QComboBox* combo, dnd::Repository& repository, const dnd::Character& giver) {
    auto players = repository.get_players();
    for (const auto& character : repository.get_characters()) {
        if (character.character_id == giver.character_id || character.dead) continue;
        combo->addItem(withPlayer(character, players), character.character_id);
    }
}

constexpr dnd::ItemCategory kCategories[] = {dnd::ItemCategory::Gear,   dnd::ItemCategory::Consumable,
                                             dnd::ItemCategory::Weapon, dnd::ItemCategory::Armor,
                                             dnd::ItemCategory::Shield, dnd::ItemCategory::Wondrous,
                                             dnd::ItemCategory::Scroll};
constexpr int kMagicFilter = -2;  // the Buy tab's "Magic items" choice

}  // namespace

ShopDialog::ShopDialog(dnd::Repository& repository, dnd::Character character,
                       const std::vector<dnd::LookupEntry>& classes, const std::vector<dnd::LookupEntry>& races,
                       QWidget* parent)
    : QDialog(parent), repository_(repository), character_(std::move(character)) {
    className_ = nameFor(classes, character_.class_id);
    raceName_ = nameFor(races, character_.race);
    // What shops sell: mundane gear, and common and uncommon magic items.
    for (auto& item : repository_.get_items()) {
        if (dnd::sold_in_shops(item)) items_.push_back(std::move(item));
    }
    setWindowTitle(QString("Shop — %1").arg(text(character_.character_name)));

    purseLabel_ = new QLabel(this);
    purseLabel_->setWordWrap(true);
    auto* giveButton = new QPushButton("Give Coins…", this);
    giveButton->setToolTip("Hand coins to another character: to pool money for a raising, or to outfit a new "
                           "character.");
    auto* header = new QHBoxLayout();
    header->addWidget(purseLabel_, 1);
    header->addWidget(giveButton);

    tabs_ = new QTabWidget(this);

    // --- Buy ---
    auto* buyPage = new QWidget(tabs_);
    categoryCombo_ = new QComboBox(buyPage);
    categoryCombo_->addItem("All items", -1);
    categoryCombo_->addItem("Magic items", kMagicFilter);
    categoryCombo_->setItemData(1, "Common and uncommon magic items. Rarer magic is found, not bought.",
                                Qt::ToolTipRole);
    for (auto category : kCategories) {
        categoryCombo_->addItem(dnd::item_category_name(category), static_cast<int>(category));
    }
    buyTable_ = makeTable({"Item", "Category", "Price", "Weight"}, buyPage);
    buyQuantity_ = new QSpinBox(buyPage);
    buyQuantity_->setRange(1, 99);
    buyButton_ = new QPushButton("Buy", buyPage);
    buyWarnings_ = new QLabel(buyPage);
    buyWarnings_->setWordWrap(true);
    buyWarnings_->setStyleSheet("color: #b00020;");
    auto* filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel("Show:", buyPage));
    filterRow->addWidget(categoryCombo_, 1);
    auto* buyRow = new QHBoxLayout();
    buyRow->addStretch(1);
    buyRow->addWidget(new QLabel("Quantity:", buyPage));
    buyRow->addWidget(buyQuantity_);
    buyRow->addWidget(buyButton_);
    auto* buyLayout = new QVBoxLayout(buyPage);
    buyLayout->addLayout(filterRow);
    buyLayout->addWidget(buyTable_, 1);
    buyLayout->addWidget(buyWarnings_);
    buyLayout->addLayout(buyRow);
    tabs_->addTab(buyPage, "Buy");

    // --- Sell ---
    auto* sellPage = new QWidget(tabs_);
    sellTable_ = makeTable({"Item", "Qty", "Pays each", "Equipped"}, sellPage);
    sellTable_->horizontalHeaderItem(2)->setToolTip("Shops pay half the list price (PHB ch. 5).");
    sellQuantity_ = new QSpinBox(sellPage);
    sellQuantity_->setRange(1, 1);
    sellButton_ = new QPushButton("Sell", sellPage);
    sellHint_ = new QLabel(sellPage);
    sellHint_->setWordWrap(true);
    auto* sellRow = new QHBoxLayout();
    sellRow->addStretch(1);
    sellRow->addWidget(new QLabel("Quantity:", sellPage));
    sellRow->addWidget(sellQuantity_);
    sellRow->addWidget(sellButton_);
    auto* sellLayout = new QVBoxLayout(sellPage);
    sellLayout->addWidget(sellTable_, 1);
    sellLayout->addWidget(sellHint_);
    sellLayout->addLayout(sellRow);
    tabs_->addTab(sellPage, "Sell");

    // --- Temple ---
    auto* templePage = new QWidget(tabs_);
    templeIntro_ = new QLabel(templePage);
    templeIntro_->setWordWrap(true);
    deadCombo_ = new QComboBox(templePage);
    payerCombo_ = new QComboBox(templePage);
    payerCombo_->setToolTip("Any character can pay, the dead one included: their purse survives them.");
    raiseButton_ = new QPushButton(QString("Raise Dead (%1)").arg(cost(dnd::kRaiseDeadCostCp)), templePage);
    raiseButton_->setToolTip("The 500 gp diamond the spell consumes, plus a 50 gp fee. The character returns with "
                             "1 HP and -4 to attack rolls, saving throws, and ability checks, easing by 1 each "
                             "long rest.");
    auto* templeForm = new QFormLayout();
    templeForm->addRow("Raise:", deadCombo_);
    templeForm->addRow("Paid by:", payerCombo_);
    auto* raiseRow = new QHBoxLayout();
    raiseRow->addStretch(1);
    raiseRow->addWidget(raiseButton_);
    auto* templeLayout = new QVBoxLayout(templePage);
    templeLayout->addWidget(templeIntro_);
    templeLayout->addLayout(templeForm);
    templeLayout->addLayout(raiseRow);
    templeLayout->addStretch(1);
    tabs_->addTab(templePage, "Temple");

    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(tabs_, 1);
    layout->addWidget(messageLabel_);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(600, 520), "Shop");

    connect(giveButton, &QPushButton::clicked, this, &ShopDialog::onGiveClicked);
    connect(categoryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { fillBuyTable(); });
    connect(buyTable_, &QTableWidget::itemSelectionChanged, this, &ShopDialog::refreshBuyControls);
    connect(buyQuantity_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ShopDialog::refreshBuyControls);
    connect(buyButton_, &QPushButton::clicked, this, &ShopDialog::onBuyClicked);
    connect(sellTable_, &QTableWidget::itemSelectionChanged, this, &ShopDialog::refreshSellControls);
    connect(sellQuantity_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ShopDialog::refreshSellControls);
    connect(sellButton_, &QPushButton::clicked, this, &ShopDialog::onSellClicked);
    connect(deadCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShopDialog::refreshTemple);
    connect(payerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShopDialog::refreshTemple);
    connect(raiseButton_, &QPushButton::clicked, this, &ShopDialog::onRaiseClicked);

    fillBuyTable();
    reload();
    if (character_.dead) tabs_->setCurrentIndex(2);
}

void ShopDialog::reload() {
    everyone_ = repository_.get_characters();
    auto players = repository_.get_players();
    for (const auto& member : everyone_) {
        if (member.character_id == character_.character_id) character_ = member;
    }
    purseLabel_->setText(QString("<b>%1</b>%2 · <b>Coins:</b> %3")
                             .arg(text(character_.character_name), character_.dead ? " (dead)" : "",
                                  text(dnd::format_coins(character_.coins))));

    // Sell: the pack, with what each fetches.
    int sellRow = sellTable_->currentRow();
    pack_ = repository_.get_inventory(character_.character_id);
    sellTable_->setRowCount(static_cast<int>(pack_.size()));
    for (int row = 0; row < static_cast<int>(pack_.size()); ++row) {
        const auto& entry = pack_[static_cast<size_t>(row)];
        auto* name = new QTableWidgetItem(text(entry.item.name));
        name->setToolTip(text(entry.item.description));
        sellTable_->setItem(row, 0, name);
        sellTable_->setItem(row, 1, new QTableWidgetItem(QString::number(entry.quantity)));
        int each = dnd::sell_price_cp(entry.item);
        sellTable_->setItem(row, 2, new QTableWidgetItem(each > 0 ? cost(each) : "—"));
        sellTable_->setItem(row, 3, new QTableWidgetItem(entry.equipped ? "✓" : ""));
    }
    if (sellRow >= 0 && sellRow < sellTable_->rowCount()) sellTable_->selectRow(sellRow);

    // Temple: the dead, and everyone who could pay.
    int deadId = deadCombo_->currentData().toInt();
    int payerId = payerCombo_->count() > 0 ? payerCombo_->currentData().toInt() : character_.character_id;
    {
        QSignalBlocker blockDead(deadCombo_);
        QSignalBlocker blockPayer(payerCombo_);
        deadCombo_->clear();
        payerCombo_->clear();
        for (const auto& member : everyone_) {
            QString name = withPlayer(member, players);
            if (member.dead) deadCombo_->addItem(name, member.character_id);
            payerCombo_->addItem(QString("%1: %2").arg(name, text(dnd::format_coins(member.coins))),
                                 member.character_id);
        }
        int dead = deadCombo_->findData(character_.dead && deadId == 0 ? character_.character_id : deadId);
        deadCombo_->setCurrentIndex(std::max(0, dead));
        payerCombo_->setCurrentIndex(std::max(0, payerCombo_->findData(payerId)));
    }

    refreshBuyControls();
    refreshSellControls();
    refreshTemple();
}

void ShopDialog::fillBuyTable() {
    int category = categoryCombo_->currentData().toInt();
    shownItems_.clear();
    for (size_t i = 0; i < items_.size(); ++i) {
        bool shown = category == -1 || (category == kMagicFilter && items_[i].magical()) ||
                     static_cast<int>(items_[i].category) == category;
        if (shown) shownItems_.push_back(static_cast<int>(i));
    }
    buyTable_->setRowCount(static_cast<int>(shownItems_.size()));
    for (int row = 0; row < static_cast<int>(shownItems_.size()); ++row) {
        const auto& item = items_[static_cast<size_t>(shownItems_[static_cast<size_t>(row)])];
        QString label = text(item.name);
        if (item.magical()) label += QString(" (%1)").arg(dnd::rarity_name(item.rarity));
        auto* name = new QTableWidgetItem(label);
        name->setToolTip(text(item.description));
        buyTable_->setItem(row, 0, name);
        buyTable_->setItem(row, 1, new QTableWidgetItem(dnd::item_category_name(item.category)));
        buyTable_->setItem(row, 2, new QTableWidgetItem(cost(item.cost_cp)));
        buyTable_->setItem(row, 3, new QTableWidgetItem(QString("%1 lb").arg(item.weight_lb)));
    }
    refreshBuyControls();
}

const dnd::Item* ShopDialog::selectedShopItem() const {
    int row = buyTable_->currentRow();
    if (buyTable_->selectedItems().isEmpty() || row < 0 || row >= static_cast<int>(shownItems_.size())) {
        return nullptr;
    }
    return &items_[static_cast<size_t>(shownItems_[static_cast<size_t>(row)])];
}

const dnd::InventoryEntry* ShopDialog::selectedPackEntry() const {
    int row = sellTable_->currentRow();
    if (sellTable_->selectedItems().isEmpty() || row < 0 || row >= static_cast<int>(pack_.size())) return nullptr;
    return &pack_[static_cast<size_t>(row)];
}

void ShopDialog::refreshBuyControls() {
    const dnd::Item* item = selectedShopItem();
    buyWarnings_->clear();
    buyButton_->setToolTip("");
    if (!item) {
        buyButton_->setText("Buy");
        buyButton_->setEnabled(false);
        return;
    }
    int total = item->cost_cp * buyQuantity_->value();
    buyButton_->setText(QString("Buy for %1").arg(cost(total)));
    int purse = dnd::total_cp(character_.coins);
    bool affordable = purse >= total;
    buyButton_->setEnabled(affordable && !character_.dead);
    if (character_.dead) {
        buyButton_->setToolTip("The dead can't go shopping. Raise them at the Temple first.");
    } else if (!affordable) {
        buyButton_->setToolTip(QString("%1 short: %2 has %3.")
                                   .arg(cost(total - purse), text(character_.character_name), cost(purse)));
    }
    QStringList warnings;
    for (const auto& warning : dnd::equipment_warnings(character_, className_, raceName_, *item)) {
        warnings << text(warning);
    }
    buyWarnings_->setText(warnings.join("\n"));
}

void ShopDialog::refreshSellControls() {
    const dnd::InventoryEntry* entry = selectedPackEntry();
    sellHint_->clear();
    if (!entry) {
        sellButton_->setText("Sell");
        sellButton_->setEnabled(false);
        sellQuantity_->setRange(1, 1);
        return;
    }
    {
        QSignalBlocker block(sellQuantity_);
        sellQuantity_->setRange(1, std::max(1, entry->quantity));
    }
    int each = dnd::sell_price_cp(entry->item);
    sellButton_->setText(QString("Sell for %1").arg(cost(each * sellQuantity_->value())));
    if (entry->equipped) {
        sellHint_->setText(QString("Unequip the %1 in the Inventory before selling it.").arg(text(entry->item.name)));
    } else if (each == 0) {
        sellHint_->setText(QString("No one will pay anything for a %1.").arg(text(entry->item.name)));
    }
    sellButton_->setEnabled(!entry->equipped && each > 0);
}

void ShopDialog::refreshTemple() {
    bool anyoneDead = deadCombo_->count() > 0;
    templeIntro_->setText(anyoneDead
        ? QString("The temple's priests can cast <i>raise dead</i> for %1. The raised return with 1 HP and -4 to "
                  "attack rolls, saving throws, and ability checks, easing by 1 each long rest.")
              .arg(cost(dnd::kRaiseDeadCostCp))
        : QString("No one needs raising."));
    deadCombo_->setEnabled(anyoneDead);
    payerCombo_->setEnabled(anyoneDead);
    raiseButton_->setToolTip("");
    if (!anyoneDead) {
        raiseButton_->setEnabled(false);
        return;
    }
    int payerId = payerCombo_->currentData().toInt();
    int purse = 0;
    for (const auto& member : everyone_) {
        if (member.character_id == payerId) purse = dnd::total_cp(member.coins);
    }
    bool affordable = purse >= dnd::kRaiseDeadCostCp;
    raiseButton_->setEnabled(affordable);
    if (!affordable) {
        raiseButton_->setToolTip(QString("%1 short. Give Coins… can pool the party's money.")
                                     .arg(cost(dnd::kRaiseDeadCostCp - purse)));
    }
}

void ShopDialog::showMessage(const std::string& message, bool ok) {
    messageLabel_->setStyleSheet(ok ? "" : "color: #b00020;");
    messageLabel_->setText(text(message));
}

void ShopDialog::onBuyClicked() {
    const dnd::Item* item = selectedShopItem();
    if (!item) return;
    auto result = dnd::buy(repository_, character_.character_id, item->item_id, buyQuantity_->value());
    showMessage(result.message, result.ok);
    reload();
}

void ShopDialog::onSellClicked() {
    const dnd::InventoryEntry* entry = selectedPackEntry();
    if (!entry) return;
    auto result = dnd::sell(repository_, character_.character_id, entry->item.item_id, sellQuantity_->value());
    showMessage(result.message, result.ok);
    reload();
}

void ShopDialog::onRaiseClicked() {
    if (deadCombo_->count() == 0) return;
    auto result = dnd::raise_dead(repository_, deadCombo_->currentData().toInt(), payerCombo_->currentData().toInt());
    showMessage(result.message, result.ok);
    reload();
}

void ShopDialog::onGiveClicked() {
    GiveCoinsDialog dialog(repository_, character_, this);
    if (dialog.exec() == QDialog::Accepted) showMessage(dialog.message(), true);
    reload();
}

// --- Give Coins ----------------------------------------------------------------

GiveCoinsDialog::GiveCoinsDialog(dnd::Repository& repository, dnd::Character giver, QWidget* parent)
    : QDialog(parent), repository_(repository), giver_(std::move(giver)) {
    setWindowTitle(QString("Give Coins — %1").arg(text(giver_.character_name)));

    recipientCombo_ = new QComboBox(this);
    fillRecipients(recipientCombo_, repository_, giver_);

    auto* form = new QFormLayout();
    form->addRow("Give to:", recipientCombo_);
    // Largest coin first, like the purse is written.
    coinSpins_.resize(dnd::kCoinCount);
    for (int i = dnd::kCoinCount - 1; i >= 0; --i) {
        auto* spin = new QSpinBox(this);
        int have = giver_.coins[static_cast<size_t>(i)];
        spin->setRange(0, have);
        spin->setSuffix(QString(" of %1").arg(have));
        spin->setEnabled(have > 0);
        coinSpins_[static_cast<size_t>(i)] = spin;
        form->addRow(QString("%1:").arg(dnd::coin_abbreviation(static_cast<dnd::Coin>(i))), spin);
    }

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");
    if (recipientCombo_->count() == 0) errorLabel_->setText("There's no one else alive to give coins to.");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Give");
    buttons->button(QDialogButtonBox::Ok)->setEnabled(recipientCombo_->count() > 0);
    connect(buttons, &QDialogButtonBox::accepted, this, &GiveCoinsDialog::give);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addStretch(1);
    layout->addWidget(buttons);
}

void GiveCoinsDialog::give() {
    if (recipientCombo_->count() == 0) return;
    dnd::Coins coins{};
    for (size_t i = 0; i < coins.size(); ++i) coins[i] = coinSpins_[i]->value();
    auto result = dnd::give_coins(repository_, giver_.character_id, recipientCombo_->currentData().toInt(), coins);
    if (!result.ok) {
        errorLabel_->setText(text(result.message));
        return;
    }
    message_ = result.message;
    accept();
}

// --- Give Item -----------------------------------------------------------------

GiveItemDialog::GiveItemDialog(dnd::Repository& repository, dnd::Character giver, dnd::InventoryEntry entry,
                               QWidget* parent)
    : QDialog(parent), repository_(repository), giver_(std::move(giver)), entry_(std::move(entry)) {
    setWindowTitle(QString("Give %1 — %2").arg(text(entry_.item.name), text(giver_.character_name)));

    recipientCombo_ = new QComboBox(this);
    fillRecipients(recipientCombo_, repository_, giver_);
    quantitySpin_ = new QSpinBox(this);
    quantitySpin_->setRange(1, std::max(1, entry_.quantity));
    quantitySpin_->setSuffix(QString(" of %1").arg(entry_.quantity));

    auto* form = new QFormLayout();
    form->addRow("Give to:", recipientCombo_);
    form->addRow("How many:", quantitySpin_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");
    if (recipientCombo_->count() == 0) errorLabel_->setText("There's no one else alive to give it to.");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Give");
    buttons->button(QDialogButtonBox::Ok)->setEnabled(recipientCombo_->count() > 0);
    connect(buttons, &QDialogButtonBox::accepted, this, &GiveItemDialog::give);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(errorLabel_);
    layout->addStretch(1);
    layout->addWidget(buttons);
}

void GiveItemDialog::give() {
    if (recipientCombo_->count() == 0) return;
    auto result = dnd::give_item(repository_, giver_.character_id, recipientCombo_->currentData().toInt(),
                                 entry_.item.item_id, quantitySpin_->value());
    if (!result.ok) {
        errorLabel_->setText(text(result.message));
        return;
    }
    message_ = result.message;
    accept();
}
