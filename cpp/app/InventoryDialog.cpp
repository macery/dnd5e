#include "InventoryDialog.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ShopDialog.hpp"
#include "WindowSizing.hpp"
#include "dnd/currency.hpp"
#include "dnd/equipment.hpp"
#include "dnd/inventory.hpp"
#include "dnd/lasting_effects.hpp"
#include "dnd/magic_items.hpp"

namespace {
std::string nameFor(const std::vector<dnd::LookupEntry>& entries, int id) {
    for (const auto& entry : entries) {
        if (entry.id == id) return entry.name;
    }
    return "";
}
}  // namespace

InventoryDialog::InventoryDialog(dnd::Repository& repository, dnd::Character character,
                                 const std::vector<dnd::LookupEntry>& classes,
                                 const std::vector<dnd::LookupEntry>& races, QWidget* parent)
    : QDialog(parent), repository_(repository), character_(std::move(character)) {
    className_ = nameFor(classes, character_.class_id);
    raceName_ = nameFor(races, character_.race);
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

    coinsLabel_ = new QLabel(this);
    coinsLabel_->setWordWrap(true);
    coinsLabel_->setToolTip("Coins carried: platinum, gold, electrum, silver, copper (1 pp = 10 gp = 20 ep = 100 sp = "
                            "1,000 cp). Adventures pay coins, split among the survivors. Spend them in the Shop.");

    equipmentLabel_ = new QLabel(this);
    equipmentLabel_->setWordWrap(true);

    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels({"Item", "Qty", "Weight", "Value", "Equipped", "Attuned"});
    table_->horizontalHeaderItem(4)->setToolTip(
        "Worn armor and shield, and the wielded weapon. They set AC and the attack used in combat.");
    table_->horizontalHeaderItem(5)->setToolTip(
        "Magic items that require attunement work only while attuned: at most 3, and attuning to one takes a short "
        "rest (choose it in the Short Rest dialog). Attuned wondrous items count as worn.");
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->setVisible(false);

    auto* rowButtons = new QHBoxLayout();
    equipButton_ = new QPushButton("Equip", this);
    equipButton_->setToolTip("Wear armor or a shield, or wield a weapon. One of each at a time; a two-handed "
                             "weapon can't be used with a shield.");
    useButton_ = new QPushButton("Use", this);
    useButton_->setToolTip("Use the selected consumable, e.g. drink a Potion of Healing to regain HP.");
    dropButton_ = new QPushButton("Drop One", this);
    giveButton_ = new QPushButton("Give…", this);  // tooltip set in onSelectionChanged
    rowButtons->addStretch(1);
    rowButtons->addWidget(equipButton_);
    rowButtons->addWidget(useButton_);
    rowButtons->addWidget(dropButton_);
    rowButtons->addWidget(giveButton_);

    auto* addRow = new QHBoxLayout();
    itemCombo_ = new QComboBox(this);
    for (const auto& item : repository_.get_items()) {
        itemCombo_->addItem(QString::fromStdString(item.name), item.item_id);
        itemCombo_->setItemData(itemCombo_->count() - 1, QString::fromStdString(item.description), Qt::ToolTipRole);
    }
    quantitySpin_ = new QSpinBox(this);
    quantitySpin_->setRange(1, 99);
    addButton_ = new QPushButton("Add (free)", this);
    addButton_->setToolTip("Put items in the pack at no cost: found loot, or a gift from the DM. Use the Shop to "
                           "buy them.");
    addRow->addWidget(itemCombo_, 1);
    addRow->addWidget(quantitySpin_);
    addRow->addWidget(addButton_);

    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(headerRow);
    layout->addWidget(coinsLabel_);
    layout->addWidget(equipmentLabel_);
    layout->addWidget(table_, 1);
    layout->addLayout(rowButtons);
    layout->addWidget(messageLabel_);
    layout->addLayout(addRow);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(560, 460), "Inventory");

    connect(addButton_, &QPushButton::clicked, this, &InventoryDialog::onAddClicked);
    connect(dropButton_, &QPushButton::clicked, this, &InventoryDialog::onDropClicked);
    connect(useButton_, &QPushButton::clicked, this, &InventoryDialog::onUseClicked);
    connect(equipButton_, &QPushButton::clicked, this, &InventoryDialog::onEquipClicked);
    connect(giveButton_, &QPushButton::clicked, this, &InventoryDialog::onGiveClicked);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &InventoryDialog::onSelectionChanged);

    reload();
}

void InventoryDialog::reload() {
    int selectedRow = table_->currentRow();
    inventory_ = repository_.get_inventory(character_.character_id);

    table_->setRowCount(static_cast<int>(inventory_.size()));
    for (int row = 0; row < static_cast<int>(inventory_.size()); ++row) {
        const auto& entry = inventory_[static_cast<size_t>(row)];
        QString name = QString::fromStdString(entry.item.name);
        if (entry.item.magical()) name += QString(" (%1)").arg(dnd::rarity_name(entry.item.rarity));
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setToolTip(QString::fromStdString(entry.item.description));
        table_->setItem(row, 0, nameItem);
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(entry.quantity)));
        table_->setItem(row, 2, new QTableWidgetItem(QString("%1 lb").arg(entry.item.weight_lb * entry.quantity)));
        table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(dnd::format_cost(entry.item.cost_cp))));
        table_->setItem(row, 4, new QTableWidgetItem(entry.equipped ? "✓" : ""));
        table_->setItem(row, 5, new QTableWidgetItem(entry.attuned ? "✓"
                                                     : entry.item.requires_attunement ? "needs attunement"
                                                                                      : ""));
    }
    if (selectedRow >= 0 && selectedRow < table_->rowCount()) table_->selectRow(selectedRow);

    hpLabel_->setText(QString("HP: %1 / %2").arg(character_.current_hp).arg(character_.max_hp));
    weightLabel_->setText(QString("Carrying: %1 lb").arg(dnd::total_weight(inventory_)));
    int itemsValue = 0;
    for (const auto& entry : inventory_) itemsValue += entry.item.cost_cp * entry.quantity;
    coinsLabel_->setText(QString("<b>Coins:</b> %1 · <b>Items worth:</b> %2 · <b>Attuned:</b> %3 of %4")
                             .arg(QString::fromStdString(dnd::format_coins(character_.coins)),
                                  QString::fromStdString(dnd::format_cp(itemsValue)))
                             .arg(dnd::attuned_count(inventory_))
                             .arg(dnd::kMaxAttunedItems));

    auto armorClass = dnd::calculate_armor_class(character_, className_, inventory_);
    auto attack = dnd::weapon_attack(character_, className_, raceName_, inventory_);
    equipmentLabel_->setText(QString("<b>AC %1</b> (%2) · <b>Attack:</b> %3 %4%5")
                                 .arg(armorClass.total)
                                 .arg(QString::fromStdString(armorClass.text()),
                                      QString::fromStdString(attack.name),
                                      attack.attack_bonus >= 0 ? QString("+%1").arg(attack.attack_bonus)
                                                               : QString::number(attack.attack_bonus),
                                      attack.proficient ? "" : " (not proficient)"));
    onSelectionChanged();
}

const dnd::InventoryEntry* InventoryDialog::selectedEntry() const {
    int row = table_->currentRow();
    if (table_->selectedItems().isEmpty() || row < 0 || row >= static_cast<int>(inventory_.size())) return nullptr;
    return &inventory_[static_cast<size_t>(row)];
}

void InventoryDialog::onSelectionChanged() {
    const dnd::InventoryEntry* entry = selectedEntry();
    bool attuned = entry && entry->attuned;
    dropButton_->setEnabled(entry != nullptr && !attuned);
    dropButton_->setToolTip(attuned ? "End attunement first (during a short rest)." : "");
    giveButton_->setEnabled(entry != nullptr && !entry->equipped && !attuned);
    giveButton_->setToolTip(entry && entry->equipped ? "Unequip it first."
                            : attuned                ? "End attunement first (during a short rest)."
                                                     : "Hand the selected item to another character: share out loot, "
                                                       "or pass a potion to whoever needs it.");
    bool equippable = entry && (entry->item.weapon || entry->item.armor);
    equipButton_->setEnabled(equippable);
    equipButton_->setText(entry && entry->equipped ? "Unequip" : "Equip");
    useButton_->setEnabled(entry != nullptr && dnd::is_healing_item(entry->item));
}

void InventoryDialog::onAddClicked() {
    if (itemCombo_->count() == 0) return;
    repository_.add_to_inventory(character_.character_id, itemCombo_->currentData().toInt(), quantitySpin_->value());
    messageLabel_->setText(QString("Added %1 × %2.").arg(quantitySpin_->value()).arg(itemCombo_->currentText()));
    reload();
}

void InventoryDialog::onGiveClicked() {
    const dnd::InventoryEntry* entry = selectedEntry();
    if (!entry || entry->equipped) return;
    GiveItemDialog dialog(repository_, character_, *entry, this);
    if (dialog.exec() == QDialog::Accepted) messageLabel_->setText(QString::fromStdString(dialog.message()));
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
    if (character_.dead) {
        messageLabel_->setText(QString("%1 is dead; a potion can't help.").arg(QString::fromStdString(character_.character_name)));
        return;
    }
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

void InventoryDialog::onEquipClicked() {
    const dnd::InventoryEntry* entry = selectedEntry();
    if (!entry) return;
    int itemId = entry->item.item_id;
    QString name = QString::fromStdString(entry->item.name);

    if (entry->equipped) {
        dnd::unequip(inventory_, itemId);
        repository_.set_equipped(character_.character_id, itemId, false);
        messageLabel_->setText(QString("Unequipped %1.").arg(name));
        reload();
        return;
    }

    auto result = dnd::equip(inventory_, itemId, character_, className_, raceName_);
    if (!result.ok) {
        messageLabel_->setText(QString::fromStdString(result.error));
        return;
    }
    repository_.set_equipped(character_.character_id, itemId, true);
    QStringList removed;
    for (int other : result.unequipped) {
        repository_.set_equipped(character_.character_id, other, false);
        for (const auto& e : inventory_) {
            if (e.item.item_id == other) removed << QString::fromStdString(e.item.name);
        }
    }
    QString message = QString("Equipped %1.").arg(name);
    if (!removed.isEmpty()) message += QString(" (Took off %1.)").arg(removed.join(", "));
    if (dnd::end_mage_armor_if_armored(character_, inventory_)) {
        repository_.update_character(character_);
        message += " Mage Armor ends.";
    }
    for (const auto& warning : result.warnings) message += "<br><span style='color:#b00020'>" +
                                                           QString::fromStdString(warning).toHtmlEscaped() + "</span>";
    messageLabel_->setText(message);
    reload();
}
