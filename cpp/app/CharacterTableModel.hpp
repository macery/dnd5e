#pragma once

#include <QAbstractTableModel>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/player.hpp"

class CharacterTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit CharacterTableModel(QObject* parent = nullptr);

    void setCharacters(std::vector<dnd::Character> characters);
    void setClasses(std::vector<dnd::LookupEntry> classes);
    void setRaces(std::vector<dnd::LookupEntry> races);
    void setAlignments(std::vector<dnd::LookupEntry> alignments);
    void setCampaigns(std::vector<dnd::Campaign> campaigns);
    void setPlayers(std::vector<dnd::Player> players);
    const dnd::Character& characterAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<dnd::Character> characters_;
    std::vector<dnd::LookupEntry> classes_;
    std::vector<dnd::LookupEntry> races_;
    std::vector<dnd::LookupEntry> alignments_;
    std::vector<dnd::Campaign> campaigns_;
    std::vector<dnd::Player> players_;
};
