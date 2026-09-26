#include "CharacterTableModel.hpp"

namespace {

constexpr int kColumnCount = 5;

QString classNameFor(const std::vector<dnd::CharacterClass>& classes, int classId) {
    for (const auto& character_class : classes) {
        if (character_class.class_id == classId) return QString::fromStdString(character_class.class_name);
    }
    return QString("Class #%1").arg(classId);
}

}  // namespace

CharacterTableModel::CharacterTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void CharacterTableModel::setCharacters(std::vector<dnd::Character> characters) {
    beginResetModel();
    characters_ = std::move(characters);
    endResetModel();
}

void CharacterTableModel::setClasses(std::vector<dnd::CharacterClass> classes) {
    beginResetModel();
    classes_ = std::move(classes);
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
    if (!index.isValid() || role != Qt::DisplayRole) return {};

    const auto& character = characters_.at(static_cast<size_t>(index.row()));
    switch (index.column()) {
        case 0: return QString::fromStdString(character.character_name);
        case 1: return classNameFor(classes_, character.class_id);
        case 2: return character.race;
        case 3: return character.alignment;
        case 4: return character.level;
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
        default: return {};
    }
}
