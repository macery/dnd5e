#include "ImportDialog.hpp"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <stdexcept>

#include "WindowSizing.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

// ", exported 1 Oct 2026 at 3:16 PM" in local time; "" if missing or unreadable.
QString exportedText(const std::string& iso) {
    QDateTime when = QDateTime::fromString(QString::fromStdString(iso), Qt::ISODate);
    if (!when.isValid()) return QString();
    return ", exported " + QLocale().toString(when.toLocalTime(), QLocale::ShortFormat);
}

}  // namespace

ImportDialog::ImportDialog(dnd::Repository& repository, dnd::CharacterExport file, QWidget* parent)
    : QDialog(parent), repository_(repository), plan_(dnd::plan_import(repository, file)) {
    setWindowTitle("Import Characters");

    auto* intro = new QLabel(this);
    intro->setWordWrap(true);
    intro->setText(QString("<b>%1</b> character%2 from <b>%3</b>%4.")
                       .arg(plan_.characters.size())
                       .arg(plan_.characters.size() == 1 ? "" : "s")
                       .arg(q(plan_.player_name).toHtmlEscaped())
                       .arg(exportedText(file.exported_at)));

    // Where they go: the file's player (matched by name, or created), or any existing player.
    playerCombo_ = new QComboBox(this);
    playerCombo_->addItem(plan_.existing_player_id != 0
                              ? QString("%1 (the file's player)").arg(q(plan_.player_name))
                              : QString("%1 (new player, from the file)").arg(q(plan_.player_name)),
                          0);
    for (const auto& player : repository_.get_players()) {
        if (player.player_id == plan_.existing_player_id) continue;
        playerCombo_->addItem(q(player.player_name), player.player_id);
    }
    auto* form = new QFormLayout();
    form->addRow("Import into", playerCombo_);

    table_ = new QTableWidget(static_cast<int>(plan_.characters.size()), 3, this);
    table_->setHorizontalHeaderLabels({"Character", "Class", "What happens"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);

    QStringList warnings;
    int importable = 0;
    for (int row = 0; row < static_cast<int>(plan_.characters.size()); ++row) {
        const auto& planned = plan_.characters[static_cast<size_t>(row)];
        const auto& source = planned.source;
        table_->setItem(row, 0, new QTableWidgetItem(q(source.name) + (source.dead ? " (dead)" : "")));
        table_->setItem(row, 1, new QTableWidgetItem(QString("Level %1 %2 %3")
                                                         .arg(source.level)
                                                         .arg(q(source.race), q(source.class_name))));
        QComboBox* conflict = nullptr;
        if (!planned.importable) {
            auto* item = new QTableWidgetItem("Can't import: " + q(planned.error));
            item->setForeground(QColor(176, 0, 32));
            table_->setItem(row, 2, item);
        } else if (planned.existing_character_id != 0) {
            conflict = new QComboBox(table_);
            conflict->addItem(QString("Import as a copy: \"%1\"").arg(q(planned.copy_name)),
                              static_cast<int>(dnd::ConflictChoice::ImportAsCopy));
            QString owner;
            for (const auto& existing : repository_.get_characters()) {
                if (existing.character_id != planned.existing_character_id) continue;
                for (const auto& player : repository_.get_players()) {
                    if (player.player_id == existing.player_id) owner = q(player.player_name) + "'s ";
                }
            }
            conflict->addItem(QString("Replace %1%2").arg(owner, q(source.name)),
                              static_cast<int>(dnd::ConflictChoice::Replace));
            conflict->addItem("Skip", static_cast<int>(dnd::ConflictChoice::Skip));
            conflict->setToolTip(QString("A character named %1 already exists. Replacing it deletes the existing "
                                         "character, with its inventory and spells.")
                                     .arg(q(source.name)));
            table_->setCellWidget(row, 2, conflict);
            ++importable;
        } else {
            table_->setItem(row, 2, new QTableWidgetItem("Import"));
            ++importable;
        }
        conflictCombos_.push_back(conflict);
        for (const auto& warning : planned.warnings) {
            warnings << (q(source.name) + ": " + q(warning)).toHtmlEscaped();
        }
    }
    table_->resizeRowsToContents();

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextFormat(Qt::RichText);
    QString summary;
    if (!warnings.isEmpty()) {
        summary += "<b>Won't come across</b> (this version doesn't have them):<br>" + warnings.join("<br>");
    }
    if (importable == 0) summary += (summary.isEmpty() ? "" : "<br><br>") + QString("<b>Nothing here can be imported.</b>");
    summaryLabel_->setText(summary);
    summaryLabel_->setVisible(!summary.isEmpty());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* importButton = buttons->addButton("Import", QDialogButtonBox::AcceptRole);
    importButton->setEnabled(importable > 0);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(form);
    layout->addWidget(table_, 1);
    layout->addWidget(summaryLabel_);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(640, 420), "Import");
}

void ImportDialog::accept() {
    dnd::ImportChoices choices;
    choices.target_player_id = playerCombo_->currentData().toInt();
    for (QComboBox* combo : conflictCombos_) {
        choices.conflicts.push_back(combo ? static_cast<dnd::ConflictChoice>(combo->currentData().toInt())
                                          : dnd::ConflictChoice::ImportAsCopy);
    }
    try {
        result_ = dnd::apply_import(repository_, plan_, choices);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Import Characters",
                             QString("The import failed, and nothing was changed:\n\n%1").arg(error.what()));
        return;
    }
    QDialog::accept();
}
