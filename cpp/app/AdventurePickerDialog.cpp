#include "AdventurePickerDialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>

#include "dnd/adventure.hpp"

AdventurePickerDialog::AdventurePickerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Solo Adventure");

    list_ = new QListWidget(this);
    for (const auto& adventure : dnd::adventure_templates()) {
        list_->addItem(QString::fromStdString(adventure.title));
    }

    hookLabel_ = new QLabel(this);
    hookLabel_->setWordWrap(true);
    hookLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* contentRow = new QHBoxLayout();
    contentRow->addWidget(list_, 1);
    contentRow->addWidget(hookLabel_, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(contentRow, 1);
    layout->addWidget(buttons);
    resize(480, 280);

    connect(list_, &QListWidget::currentRowChanged, this, &AdventurePickerDialog::onSelectionChanged);
    if (list_->count() > 0) list_->setCurrentRow(0);
}

int AdventurePickerDialog::selectedIndex() const {
    return list_->currentRow();
}

void AdventurePickerDialog::onSelectionChanged(int index) {
    const auto& templates = dnd::adventure_templates();
    if (index < 0 || index >= static_cast<int>(templates.size())) {
        hookLabel_->clear();
        return;
    }
    const auto& adventure = templates[static_cast<size_t>(index)];
    QString text = QString::fromStdString(adventure.hook);
    if (!adventure.loot.empty()) {
        QStringList rewards;
        for (const auto& loot : adventure.loot) {
            rewards << QString("%1 × %2").arg(loot.quantity).arg(QString::fromStdString(loot.item_name));
        }
        text += "\n\nReward: " + rewards.join(", ");
    }
    hookLabel_->setText(text);
}
