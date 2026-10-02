#include "CharacterTableModel.hpp"

#include <QBrush>
#include <QFont>
#include <QColor>

#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"

namespace {

constexpr int kColumnCount = 7;

QString nameFor(const std::vector<dnd::LookupEntry>& entries, int id) {
    for (const auto& entry : entries) {
        if (entry.id == id) return QString::fromStdString(entry.name);
    }
    return QString("#%1").arg(id);
}

}  // namespace

CharacterTableModel::CharacterTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void CharacterTableModel::setCharacters(std::vector<dnd::Character> characters) {
    beginResetModel();
    characters_ = std::move(characters);
    endResetModel();
}

void CharacterTableModel::setClasses(std::vector<dnd::LookupEntry> classes) {
    beginResetModel();
    classes_ = std::move(classes);
    endResetModel();
}

void CharacterTableModel::setRaces(std::vector<dnd::LookupEntry> races) {
    beginResetModel();
    races_ = std::move(races);
    endResetModel();
}

void CharacterTableModel::setAlignments(std::vector<dnd::LookupEntry> alignments) {
    beginResetModel();
    alignments_ = std::move(alignments);
    endResetModel();
}

const dnd::Character& CharacterTableModel::characterAt(int row) const {
    return characters_.at(static_cast<size_t>(row));
}

int CharacterTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(characters_.size());
}

int CharacterTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return kColumnCount;
}

QVariant CharacterTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    const auto& character = characters_.at(static_cast<size_t>(index.row()));

    if (role == Qt::ForegroundRole && character.dead) return QBrush(QColor(128, 128, 128));
    if (role == Qt::ToolTipRole && index.column() == 6) {
        if (character.dead) return "Dead: failed three death saves or took massive damage. Only revival magic "
                                   "can bring them back: a temple's raise dead, in the Shop. They can't fight or rest.";
        if (character.current_hp <= 0) return "Unconscious at 0 HP (stable). Any healing wakes them.";
        return {};
    }
    if (role == Qt::ToolTipRole && index.column() == 4 && dnd::pending_levels(character) > 0) {
        return "Ready to level up: select this character and click Level Up.";
    }
    if (role == Qt::FontRole && index.column() == 4 && dnd::pending_levels(character) > 0) {
        QFont bold;
        bold.setBold(true);
        return bold;
    }
    if (role != Qt::DisplayRole) return {};

    switch (index.column()) {
        case 0: return QString::fromStdString(character.character_name);
        case 1: return nameFor(classes_, character.class_id);
        case 2: return nameFor(races_, character.race);
        case 3: return nameFor(alignments_, character.alignment);
        case 4:
            if (dnd::pending_levels(character) > 0) {
                return QString("%1 → %2").arg(character.level).arg(dnd::level_for_xp(character.experience));
            }
            return character.level;
        case 5: return QString("%1/%2").arg(character.current_hp).arg(character.max_hp);
        case 6:
            if (character.dead) return "Dead";
            if (character.current_hp <= 0) return "Unconscious";
            return "";
        default: return {};
    }
}

QVariant CharacterTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};

    switch (section) {
        case 0: return "Name";
        case 1: return "Class";
        case 2: return "Race";
        case 3: return "Alignment";
        case 4: return "Level";
        case 5: return "HP";
        case 6: return "Status";
        default: return {};
    }
}
