#include "InventoryDialog.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "dnd/inventory.hpp"

InventoryDialog::InventoryDialog(dnd::Repository& repository, dnd::Character character, QWidget* parent)
    : QDialog(parent), repository_(repository), character_(std::move(character)) {
    setWindowTitle(QString("Inventory — %1").arg(QString::fromStdString(character_.character_name)));

    hpLabel_ = new QLabel(this);
    weightLabel_ = new QLabel(this);
    weightLabel_->setToolTip(QString("Total weight of everything carried. Carrying capacity is Strength × 15 = %1 lb "
                                     "(not enforced yet).")
                                 .arg(character_.strength * 15));
    auto* headerRow = new QHBoxLayout();
    headerRow->addWidget(hpLabel_);
    headerRow->addStretch(1);
    headerRow->addWidget(weightLabel_);

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Item", "Qty", "Weight", "Value"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->setVisible(false);

    auto* rowButtons = new QHBoxLayout();
    useButton_ = new QPushButton("Use", this);
    useButton_->setToolTip("Use the selected consumable, e.g. drink a Potion of Healing to regain HP.");
    dropButton_ = new QPushButton("Drop One", this);
    rowButtons->addStretch(1);
    rowButtons->addWidget(useButton_);
    rowButtons->addWidget(dropButton_);

    auto* addRow = new QHBoxLayout();
    itemCombo_ = new QComboBox(this);
    for (const auto& item : repository_.get_items()) {
        itemCombo_->addItem(QString::fromStdString(item.name), item.item_id);
        itemCombo_->setItemData(itemCombo_->count() - 1, QString::fromStdString(item.description), Qt::ToolTipRole);
    }
    quantitySpin_ = new QSpinBox(this);
    quantitySpin_->setRange(1, 99);
    addButton_ = new QPushButton("Add", this);
    addRow->addWidget(itemCombo_, 1);
    addRow->addWidget(quantitySpin_);
    addRow->addWidget(addButton_);

    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(headerRow);
    layout->addWidget(table_, 1);
    layout->addLayout(rowButtons);
    layout->addWidget(messageLabel_);
    layout->addLayout(addRow);
    layout->addWidget(buttons);
    resize(460, 420);

    connect(addButton_, &QPushButton::clicked, this, &InventoryDialog::onAddClicked);
    connect(dropButton_, &QPushButton::clicked, this, &InventoryDialog::onDropClicked);
    connect(useButton_, &QPushButton::clicked, this, &InventoryDialog::onUseClicked);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &InventoryDialog::onSelectionChanged);

    reload();
}

void InventoryDialog::reload() {
    int selectedRow = table_->currentRow();
    inventory_ = repository_.get_inventory(character_.character_id);

    table_->setRowCount(static_cast<int>(inventory_.size()));
    for (int row = 0; row < static_cast<int>(inventory_.size()); ++row) {
        const auto& entry = inventory_[static_cast<size_t>(row)];
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(entry.item.name));
        nameItem->setToolTip(QString::fromStdString(entry.item.description));
        table_->setItem(row, 0, nameItem);
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(entry.quantity)));
        table_->setItem(row, 2, new QTableWidgetItem(QString("%1 lb").arg(entry.item.weight_lb * entry.quantity)));
        table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(dnd::format_cost(entry.item.cost_cp))));
    }
    if (selectedRow >= 0 && selectedRow < table_->rowCount()) table_->selectRow(selectedRow);

    hpLabel_->setText(QString("HP: %1 / %2").arg(character_.current_hp).arg(character_.max_hp));
    weightLabel_->setText(QString("Carrying: %1 lb").arg(dnd::total_weight(inventory_)));
    onSelectionChanged();
}

const dnd::InventoryEntry* InventoryDialog::selectedEntry() const {
    int row = table_->currentRow();
    if (table_->selectedItems().isEmpty() || row < 0 || row >= static_cast<int>(inventory_.size())) return nullptr;
    return &inventory_[static_cast<size_t>(row)];
}

void InventoryDialog::onSelectionChanged() {
    const dnd::InventoryEntry* entry = selectedEntry();
    dropButton_->setEnabled(entry != nullptr);
    useButton_->setEnabled(entry != nullptr && dnd::is_healing_item(entry->item));
}

void InventoryDialog::onAddClicked() {
    if (itemCombo_->count() == 0) return;
    repository_.add_to_inventory(character_.character_id, itemCombo_->currentData().toInt(), quantitySpin_->value());
    messageLabel_->setText(QString("Added %1 × %2.").arg(quantitySpin_->value()).arg(itemCombo_->currentText()));
    reload();
}

void InventoryDialog::onDropClicked() {
    const dnd::InventoryEntry* entry = selectedEntry();
    if (!entry) return;
    QString name = QString::fromStdString(entry->item.name);
    repository_.remove_from_inventory(character_.character_id, entry->item.item_id);
    messageLabel_->setText(QString("Dropped one %1.").arg(name));
    reload();
}

void InventoryDialog::onUseClicked() {
    const dnd::InventoryEntry* entry = selectedEntry();
    if (!entry || !dnd::is_healing_item(entry->item)) return;

    dnd::Item item = entry->item;  // reload() below invalidates `entry`
    if (character_.current_hp >= character_.max_hp) {
        messageLabel_->setText(QString("%1 is already at full HP.").arg(QString::fromStdString(character_.character_name)));
        return;
    }

    int regained = dnd::use_healing_item(character_, dice_, item);
    repository_.remove_from_inventory(character_.character_id, item.item_id);
    repository_.update_character(character_);
    messageLabel_->setText(QString("Used %1: regained %2 HP.").arg(QString::fromStdString(item.name)).arg(regained));
    reload();
}
