#pragma once

#include <QAbstractTableModel>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_class.hpp"

class CharacterTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit CharacterTableModel(QObject* parent = nullptr);

    void setCharacters(std::vector<dnd::Character> characters);
    void setClasses(std::vector<dnd::CharacterClass> classes);
    const dnd::Character& characterAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<dnd::Character> characters_;
    std::vector<dnd::CharacterClass> classes_;
};
