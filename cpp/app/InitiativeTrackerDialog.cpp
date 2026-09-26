#include "InitiativeTrackerDialog.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "dnd/rules.hpp"

namespace {
struct InitiativeEntry {
    QString name;
    int roll;
};
}  // namespace

InitiativeTrackerDialog::InitiativeTrackerDialog(std::vector<dnd::Character> characters, QWidget* parent)
    : QDialog(parent), dice_(randomSource_), characters_(std::move(characters)) {
    setWindowTitle("Initiative Tracker");

    table_ = new QTableWidget(static_cast<int>(characters_.size()), 2, this);
    table_->setHorizontalHeaderLabels({"Name", "Initiative"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int row = 0; row < static_cast<int>(characters_.size()); ++row) {
        table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(characters_[row].character_name)));
        table_->setItem(row, 1, new QTableWidgetItem("-"));
    }

    rollAllButton_ = new QPushButton("Roll All Initiative", this);
    connect(rollAllButton_, &QPushButton::clicked, this, &InitiativeTrackerDialog::onRollAllClicked);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table_, 1);
    layout->addWidget(rollAllButton_);
    resize(320, 400);
}

void InitiativeTrackerDialog::onRollAllClicked() {
    std::vector<InitiativeEntry> entries;
    entries.reserve(characters_.size());
    for (const auto& character : characters_) {
        int d20 = dice_.roll(20, 1).front();
        int total = d20 + dnd::initiative(character);
        entries.push_back({QString::fromStdString(character.character_name), total});
    }

    std::sort(entries.begin(), entries.end(), [](const InitiativeEntry& a, const InitiativeEntry& b) {
        return a.roll > b.roll;
    });

    for (int row = 0; row < static_cast<int>(entries.size()); ++row) {
        table_->setItem(row, 0, new QTableWidgetItem(entries[row].name));
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(entries[row].roll)));
    }
}
