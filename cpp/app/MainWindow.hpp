#pragma once

#include <QMainWindow>
#include <vector>

#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableView;
class QModelIndex;
class CharacterTableModel;
class DiceRollerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(dnd::Repository& repository, QWidget* parent = nullptr);

private slots:
    void onPlayerSelected(int index);
    void onAddPlayerClicked();
    void onEditPlayerClicked();
    void onDeletePlayerClicked();
    void onAddCharacterClicked();
    void onEditCharacterClicked();
    void onDeleteCharacterClicked();
    void onCharacterActivated(const QModelIndex& index);

private:
    void reloadPlayers();
    void reloadLookupTables();
    void reloadCharactersForCurrentPlayer();
    int currentPlayerId() const;
    QString currentPlayerName() const;

    dnd::Repository& repository_;
    std::vector<dnd::LookupEntry> classes_;
    std::vector<dnd::LookupEntry> races_;
    std::vector<dnd::LookupEntry> alignments_;

    QComboBox* playerCombo_ = nullptr;
    QLineEdit* newPlayerName_ = nullptr;
    QPushButton* addPlayerButton_ = nullptr;
    QPushButton* editPlayerButton_ = nullptr;
    QPushButton* deletePlayerButton_ = nullptr;
    QTableView* characterTable_ = nullptr;
    QPushButton* addCharacterButton_ = nullptr;
    QPushButton* editCharacterButton_ = nullptr;
    QPushButton* deleteCharacterButton_ = nullptr;
    CharacterTableModel* characterModel_ = nullptr;
    DiceRollerWidget* diceRoller_ = nullptr;
};
