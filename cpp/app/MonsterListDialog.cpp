#include "MonsterListDialog.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

#include "dnd/damage.hpp"
#include "dnd/effects.hpp"
#include "dnd/rules.hpp"

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

    static const QStringList kHeaders = {"Name", "Type", "CR", "AC", "HP", "Attacks", "Traits",
                                         "STR", "DEX", "CON", "INT", "WIS", "CHA"};

    auto* table = new QTableWidget(static_cast<int>(monsters.size()), kHeaders.size(), this);
    table->setHorizontalHeaderLabels(kHeaders);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setWordWrap(true);
    table->horizontalHeaderItem(5)->setToolTip("Every attack in a turn: to hit, damage, and type. "
                                               "On a hit, some also force a save.");
    table->horizontalHeaderItem(6)->setToolTip(
        "Damage it resists (halved), is vulnerable to (doubled), or is immune to, and conditions it can't have.");

    for (int row = 0; row < static_cast<int>(monsters.size()); ++row) {
        const auto& monster = monsters[static_cast<size_t>(row)];
        QStringList attacks;
        for (const auto& attack : monster.attacks) {
            QString text = QString("%1%2 %3, %4d%5%6 %7")
                               .arg(attack.count > 1 ? QString("%1 × ").arg(attack.count) : QString())
                               .arg(QString::fromStdString(attack.name))
                               .arg(attack.attack_bonus >= 0 ? QString("+%1").arg(attack.attack_bonus)
                                                             : QString::number(attack.attack_bonus))
                               .arg(attack.damage_dice_count)
                               .arg(attack.damage_dice_sides)
                               .arg(attack.damage_bonus > 0   ? QString("+%1").arg(attack.damage_bonus)
                                    : attack.damage_bonus < 0 ? QString::number(attack.damage_bonus)
                                                              : QString())
                               .arg(QString::fromStdString(attack.damage_type));
            if (attack.extra_dice_count > 0) {
                text += QString(" + %1d%2 %3").arg(attack.extra_dice_count).arg(attack.extra_dice_sides)
                            .arg(QString::fromStdString(attack.extra_damage_type));
            }
            if (attack.on_hit_save_dc > 0) {
                text += QString(" (DC %1 %2 or %3)").arg(attack.on_hit_save_dc)
                            .arg(dnd::ability_name(attack.on_hit_save_ability))
                            .arg(QString(dnd::condition_name(attack.on_hit_condition)).toLower());
            }
            // Without Multiattack only the first attack is made; the rest are alternatives.
            if (monster.multiattack.empty() && !attacks.isEmpty()) text = "or " + text + " (not used yet)";
            attacks << text;
        }
        if (!monster.multiattack.empty()) attacks.prepend("Multiattack: " + QString::fromStdString(monster.multiattack));

        QStringList traits;
        for (const auto& trait : monster.traits) {
            if (trait == "PackTactics") traits << "Pack Tactics (advantage while an ally fights)";
            else if (trait == "MartialAdvantage") traits << "Martial Advantage (+2d6 once a turn with an ally)";
            else if (trait == "UndeadFortitude") traits << "Undead Fortitude (CON save to stay at 1 HP)";
            else if (trait == "Regeneration") traits << "Regeneration (10 HP a turn; not after fire or acid)";
            else traits << QString::fromStdString(trait);
        }
        const auto& damage = monster.damage;
        if (!damage.resistances.empty()) traits << "Resists " + QString::fromStdString(dnd::join_list(damage.resistances));
        if (!damage.vulnerabilities.empty()) {
            traits << "Vulnerable to " + QString::fromStdString(dnd::join_list(damage.vulnerabilities));
        }
        if (!damage.immunities.empty()) traits << "Immune to " + QString::fromStdString(dnd::join_list(damage.immunities));
        if (!monster.condition_immunities.empty()) {
            QStringList names;
            for (auto condition : monster.condition_immunities) names << dnd::condition_name(condition);
            traits << "Can't be " + names.join(", ");
        }

        table->setItem(row, 0, cell(QString::fromStdString(monster.name)));
        table->setItem(row, 1, cell(QString::fromStdString(monster.creature_type)));
        table->setItem(row, 2, cell(formatChallengeRating(monster.challenge_rating)));
        table->setItem(row, 3, cell(QString::number(monster.armor_class)));
        table->setItem(row, 4, cell(QString::number(monster.hit_points)));
        table->setItem(row, 5, cell(attacks.join("\n")));
        table->setItem(row, 6, cell(traits.join("\n")));
        int abilities[] = {monster.strength, monster.dexterity, monster.constitution,
                           monster.intelligence, monster.wisdom, monster.charisma};
        for (int a = 0; a < 6; ++a) table->setItem(row, 7 + a, cell(QString::number(abilities[a])));
    }
    table->resizeColumnsToContents();
    table->resizeRowsToContents();

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table);
    resize(900, 360);
}
