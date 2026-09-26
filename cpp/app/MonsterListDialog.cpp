#include "MonsterListDialog.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString formatChallengeRating(double cr) {
    if (cr == 0.125) return "1/8";
    if (cr == 0.25) return "1/4";
    if (cr == 0.5) return "1/2";
    return QString::number(cr, 'g', 3);
}

QTableWidgetItem* cell(const QString& text) {
    return new QTableWidgetItem(text);
}

}  // namespace

MonsterListDialog::MonsterListDialog(std::vector<dnd::Monster> monsters, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Monsters");

    static const QStringList kHeaders = {"Name", "AC", "HP", "CR", "STR", "DEX", "CON", "INT", "WIS", "CHA"};

    auto* table = new QTableWidget(static_cast<int>(monsters.size()), kHeaders.size(), this);
    table->setHorizontalHeaderLabels(kHeaders);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int row = 0; row < static_cast<int>(monsters.size()); ++row) {
        const auto& monster = monsters[static_cast<size_t>(row)];
        table->setItem(row, 0, cell(QString::fromStdString(monster.name)));
        table->setItem(row, 1, cell(QString::number(monster.armor_class)));
        table->setItem(row, 2, cell(QString::number(monster.hit_points)));
        table->setItem(row, 3, cell(formatChallengeRating(monster.challenge_rating)));
        table->setItem(row, 4, cell(QString::number(monster.strength)));
        table->setItem(row, 5, cell(QString::number(monster.dexterity)));
        table->setItem(row, 6, cell(QString::number(monster.constitution)));
        table->setItem(row, 7, cell(QString::number(monster.intelligence)));
        table->setItem(row, 8, cell(QString::number(monster.wisdom)));
        table->setItem(row, 9, cell(QString::number(monster.charisma)));
    }

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table);
    resize(520, 320);
}
