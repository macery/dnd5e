#pragma once

#include <QMainWindow>
#include <vector>

#include "dnd/character_class.hpp"
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
    void onAddCharacterClicked();
    void onCharacterActivated(const QModelIndex& index);

private:
    void reloadPlayers();
    void reloadClasses();
    void reloadCharactersForCurrentPlayer();
    int currentPlayerId() const;

    dnd::Repository& repository_;
    std::vector<dnd::CharacterClass> classes_;

    QComboBox* playerCombo_ = nullptr;
    QLineEdit* newPlayerName_ = nullptr;
    QPushButton* addPlayerButton_ = nullptr;
    QTableView* characterTable_ = nullptr;
    QPushButton* addCharacterButton_ = nullptr;
    CharacterTableModel* characterModel_ = nullptr;
    DiceRollerWidget* diceRoller_ = nullptr;
};
