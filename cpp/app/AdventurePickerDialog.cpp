#include "AdventurePickerDialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>

#include "WindowSizing.hpp"
#include "dnd/adventure.hpp"
#include "dnd/currency.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

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
                                             std::vector<dnd::AdventureRun> in_progress, QWidget* parent)
    : QDialog(parent), runs_(std::move(in_progress)) {
    setWindowTitle("Solo Adventure");

    list_ = new QListWidget(this);
    for (const auto& run : runs_) {
        const dnd::AdventureTemplate* adventure = dnd::find_adventure(run.title);
        QString where = adventure ? q(dnd::progress_text(run, *adventure)) : QString("unknown adventure");
        auto* item = new QListWidgetItem(QString("▶ Resume: %1 — %2").arg(q(run.title), where), list_);
        QFont bold = item->font();
        bold.setBold(true);
        item->setFont(bold);
    }
    for (const auto& adventure : dnd::adventure_templates()) {
        scaled_.push_back(dnd::scale_adventure(adventure, party_levels, monsters));
        int fights = static_cast<int>(adventure.scenes.size());
        list_->addItem(QString("%1 — %2 · %3 · level %4+")
                           .arg(q(adventure.title), dnd::difficulty_name(scaled_.back().hardest))
                           .arg(fights == 1 ? QString("1 fight") : QString("%1 fights").arg(fights))
                           .arg(adventure.suggested_level));
    }
    list_->setMinimumWidth(list_->sizeHintForColumn(0) + 24);

    detailsLabel_ = new QLabel(this);
    detailsLabel_->setWordWrap(true);
    detailsLabel_->setTextFormat(Qt::RichText);
    detailsLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QStringList levels;
    for (int level : party_levels) levels << QString::number(level);
    auto* partyLabel = new QLabel(
        party_levels.empty()
            ? QString("No characters to rate against yet.")
            : QString("Difficulty is for your characters (level%1 %2); you choose the party next. Each fight is "
                      "scaled toward Medium to Hard, and the final fight of a longer adventure toward Hard.")
                  .arg(party_levels.size() == 1 ? "" : "s", levels.join(", ")),
        this);
    partyLabel->setWordWrap(true);
    partyLabel->setToolTip(
        "From the 2014 encounter-building rules: the monsters' XP, multiplied for how many there are, against "
        "your party's Easy / Medium / Hard / Deadly thresholds. Deadly fights can kill characters.");

    auto* contentRow = new QHBoxLayout();
    contentRow->addWidget(list_, 1);
    contentRow->addWidget(scrollable(detailsLabel_, this), 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(partyLabel);
    layout->addLayout(contentRow, 1);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(900, 480), "AdventurePicker");

    connect(list_, &QListWidget::currentRowChanged, this, &AdventurePickerDialog::onSelectionChanged);
    if (list_->count() > 0) list_->setCurrentRow(0);
}

bool AdventurePickerDialog::isResume() const {
    return list_->currentRow() >= 0 && list_->currentRow() < static_cast<int>(runs_.size());
}

const dnd::AdventureRun& AdventurePickerDialog::selectedRun() const {
    return runs_.at(static_cast<size_t>(list_->currentRow()));
}

const dnd::AdventureTemplate& AdventurePickerDialog::selectedAdventure() const {
    int index = std::max(0, list_->currentRow() - static_cast<int>(runs_.size()));
    return dnd::adventure_templates().at(static_cast<size_t>(index));
}

void AdventurePickerDialog::onSelectionChanged(int index) {
    if (index < 0) {
        detailsLabel_->clear();
        return;
    }
    if (index < static_cast<int>(runs_.size())) {
        const auto& run = runs_[static_cast<size_t>(index)];
        const dnd::AdventureTemplate* adventure = dnd::find_adventure(run.title);
        if (!adventure) {
            detailsLabel_->setText("This adventure isn't in this version of the app.");
            return;
        }
        QString text = QString("<p><b>In progress</b> since %1: %2 of %3 fights won.</p>")
                           .arg(q(run.started_at.substr(0, 10)))
                           .arg(run.next_scene)
                           .arg(adventure->scenes.size());
        text += "<p>Resuming picks up between fights, with the same party. Your characters are as you left them.</p>";
        const dnd::AdventureScene* next = dnd::next_scene(run, *adventure);
        if (next) text += QString("<p><b>Next:</b> %1</p>").arg(q(next->title).toHtmlEscaped());
        detailsLabel_->setText(text);
        return;
    }

    size_t which = static_cast<size_t>(index) - runs_.size();
    const auto& scaled = scaled_[which];
    const auto& original = dnd::adventure_templates()[which];
    QString text = "<p>" + q(original.hook).toHtmlEscaped() + "</p>";
    int coins = 0;
    for (size_t s = 0; s < scaled.adventure.scenes.size(); ++s) {
        const auto& scene = scaled.adventure.scenes[s];
        const auto& rating = scaled.ratings[s];
        coins += scene.coins_cp;
        QStringList foes;
        for (size_t g = 0; g < scene.monsters.size(); ++g) {
            const auto& group = scene.monsters[g];
            QString entry = QString("%1 × %2").arg(group.count).arg(q(group.monster_name));
            int was = original.scenes[s].monsters[g].count;
            if (was != group.count) entry += QString(" <i>(was %1)</i>").arg(was);
            foes << entry;
        }
        QString heading = original.scenes.size() == 1 ? QString("Monsters")
                                                      : QString("%1. %2").arg(s + 1).arg(q(scene.title).toHtmlEscaped());
        text += QString("<p><b>%1:</b> %2 — <span style='color:%3'>%4</span>%5%6</p>")
                    .arg(heading, foes.join(", "), colorFor(rating.difficulty), dnd::difficulty_name(rating.difficulty))
                    .arg(scene.boss ? " <i>(final fight)</i>" : "")
                    .arg(scene.camp_after ? "<br><i>Then a camp: a long rest is possible.</i>" : "");
    }
    QStringList rewards;
    if (coins > 0) rewards << q(dnd::format_cp(coins)) + " in all";
    for (const auto& scene : scaled.adventure.scenes) {
        for (const auto& loot : scene.loot) rewards << QString("%1 × %2").arg(loot.quantity).arg(q(loot.item_name));
    }
    if (!rewards.isEmpty()) text += "<p><b>Rewards:</b> " + rewards.join(", ") + "</p>";
    detailsLabel_->setText(text);
}
