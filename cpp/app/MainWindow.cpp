#include "MainWindow.hpp"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include "AddCharacterDialog.hpp"
#include "CharacterSheetDialog.hpp"
#include "CharacterTableModel.hpp"
#include "DiceRollerWidget.hpp"

MainWindow::MainWindow(dnd::Repository& repository, QWidget* parent)
    : QMainWindow(parent), repository_(repository) {
    setWindowTitle("DnD Engine");

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    auto* playerRow = new QHBoxLayout();
    playerCombo_ = new QComboBox(central);
    newPlayerName_ = new QLineEdit(central);
    newPlayerName_->setPlaceholderText("New player name");
    addPlayerButton_ = new QPushButton("Add Player", central);
    playerRow->addWidget(playerCombo_, 1);
    playerRow->addWidget(newPlayerName_, 1);
    playerRow->addWidget(addPlayerButton_);
    rootLayout->addLayout(playerRow);

    characterModel_ = new CharacterTableModel(central);
    characterTable_ = new QTableView(central);
    characterTable_->setModel(characterModel_);
    characterTable_->horizontalHeader()->setStretchLastSection(true);
    rootLayout->addWidget(characterTable_, 1);

    addCharacterButton_ = new QPushButton("Add Character", central);
    rootLayout->addWidget(addCharacterButton_);

    diceRoller_ = new DiceRollerWidget(central);
    rootLayout->addWidget(diceRoller_);

    setCentralWidget(central);
    resize(640, 480);

    connect(playerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPlayerSelected);
    connect(addPlayerButton_, &QPushButton::clicked, this, &MainWindow::onAddPlayerClicked);
    connect(addCharacterButton_, &QPushButton::clicked, this, &MainWindow::onAddCharacterClicked);
    connect(characterTable_, &QTableView::doubleClicked, this, &MainWindow::onCharacterActivated);

    reloadLookupTables();
    reloadPlayers();
}

int MainWindow::currentPlayerId() const {
    return playerCombo_->currentData().toInt();
}

void MainWindow::reloadPlayers() {
    playerCombo_->blockSignals(true);
    playerCombo_->clear();
    for (const auto& player : repository_.get_players()) {
        playerCombo_->addItem(QString::fromStdString(player.player_name), player.player_id);
    }
    playerCombo_->blockSignals(false);
    reloadCharactersForCurrentPlayer();
}

void MainWindow::reloadLookupTables() {
    classes_ = repository_.get_classes();
    races_ = repository_.get_races();
    alignments_ = repository_.get_alignments();
    characterModel_->setClasses(classes_);
    characterModel_->setRaces(races_);
    characterModel_->setAlignments(alignments_);
}

void MainWindow::reloadCharactersForCurrentPlayer() {
    if (playerCombo_->count() == 0) {
        characterModel_->setCharacters({});
        return;
    }
    characterModel_->setCharacters(repository_.get_characters_by_player(currentPlayerId()));
}

void MainWindow::onPlayerSelected(int /*index*/) {
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onAddPlayerClicked() {
    auto name = newPlayerName_->text().trimmed();
    if (name.isEmpty()) return;

    repository_.add_player(name.toStdString());
    newPlayerName_->clear();
    reloadPlayers();
}

void MainWindow::onAddCharacterClicked() {
    if (playerCombo_->count() == 0) {
        QMessageBox::information(this, "Add Character", "Add a player first.");
        return;
    }

    AddCharacterDialog dialog(classes_, races_, alignments_, this);
    if (dialog.exec() == QDialog::Accepted) {
        dnd::Character character = dialog.character();
        character.player_id = currentPlayerId();
        repository_.add_character(character);
        reloadCharactersForCurrentPlayer();
    }
}

void MainWindow::onCharacterActivated(const QModelIndex& index) {
    if (!index.isValid()) return;

    const dnd::Character& character = characterModel_->characterAt(index.row());
    CharacterSheetDialog dialog(character, classes_, races_, alignments_, this);
    dialog.exec();
}
