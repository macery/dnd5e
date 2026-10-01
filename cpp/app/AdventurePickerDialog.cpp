#include "AdventurePickerDialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>

#include "dnd/adventure.hpp"

namespace {

QString colorFor(dnd::Difficulty difficulty) {
    switch (difficulty) {
        case dnd::Difficulty::Deadly: return "#b00020";
        case dnd::Difficulty::Hard: return "#c05000";
        case dnd::Difficulty::Medium: return "#2e7d32";
        default: return "#606060";
    }
}

}  // namespace

AdventurePickerDialog::AdventurePickerDialog(std::vector<int> party_levels, std::vector<dnd::Monster> monsters,
                                             QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Solo Adventure");

    list_ = new QListWidget(this);
    for (const auto& adventure : dnd::adventure_templates()) {
        scaled_.push_back(dnd::scale_adventure(adventure, party_levels, monsters));
        const auto& scaled = scaled_.back();
        list_->addItem(QString("%1 — %2").arg(QString::fromStdString(adventure.title),
                                              dnd::difficulty_name(scaled.rating.difficulty)));
    }

    list_->setMinimumWidth(list_->sizeHintForColumn(0) + 24);  // room for "Title — Difficulty"

    detailsLabel_ = new QLabel(this);
    detailsLabel_->setWordWrap(true);
    detailsLabel_->setTextFormat(Qt::RichText);
    detailsLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QStringList levels;
    for (int level : party_levels) levels << QString::number(level);
    auto* partyLabel = new QLabel(
        party_levels.empty()
            ? QString("No characters to rate against yet.")
            : QString("Difficulty is for your party (level%1 %2). Monster counts are adjusted toward a Medium to "
                      "Hard fight.")
                  .arg(party_levels.size() == 1 ? "" : "s", levels.join(", ")),
        this);
    partyLabel->setWordWrap(true);
    partyLabel->setToolTip(
        "From the 2014 encounter-building rules: the monsters' XP, multiplied for how many there are, against "
        "your party's Easy / Medium / Hard / Deadly thresholds. Deadly fights can kill characters.");

    auto* contentRow = new QHBoxLayout();
    contentRow->addWidget(list_, 1);
    contentRow->addWidget(detailsLabel_, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(partyLabel);
    layout->addLayout(contentRow, 1);
    layout->addWidget(buttons);
    resize(760, 360);

    connect(list_, &QListWidget::currentRowChanged, this, &AdventurePickerDialog::onSelectionChanged);
    if (list_->count() > 0) list_->setCurrentRow(0);
}

const dnd::ScaledAdventure& AdventurePickerDialog::selectedAdventure() const {
    return scaled_.at(static_cast<size_t>(std::max(0, list_->currentRow())));
}

void AdventurePickerDialog::onSelectionChanged(int index) {
    if (index < 0 || index >= static_cast<int>(scaled_.size())) {
        detailsLabel_->clear();
        return;
    }
    const auto& scaled = scaled_[static_cast<size_t>(index)];
    const auto& original = dnd::adventure_templates()[static_cast<size_t>(index)];
    QString text = "<p>" + QString::fromStdString(scaled.adventure.hook).toHtmlEscaped() + "</p>";

    QStringList foes;
    for (size_t i = 0; i < scaled.adventure.monster_groups.size(); ++i) {
        const auto& group = scaled.adventure.monster_groups[i];
        QString entry = QString("%1 × %2").arg(group.count).arg(QString::fromStdString(group.monster_name));
        int was = original.monster_groups[i].count;
        if (was != group.count) entry += QString(" <i>(was %1)</i>").arg(was);
        foes << entry;
    }
    text += "<p><b>Monsters:</b> " + foes.join(", ") + "</p>";
    text += QString("<p><b>Difficulty:</b> <span style='color:%1'>%2</span></p>")
                .arg(colorFor(scaled.rating.difficulty), QString::fromStdString(scaled.rating.text()).toHtmlEscaped());

    if (!scaled.adventure.loot.empty()) {
        QStringList rewards;
        for (const auto& loot : scaled.adventure.loot) {
            rewards << QString("%1 × %2").arg(loot.quantity).arg(QString::fromStdString(loot.item_name));
        }
        text += "<p><b>Reward:</b> " + rewards.join(", ") + "</p>";
    }
    detailsLabel_->setText(text);
}
