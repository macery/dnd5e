#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include "CharacterEditorDialog.hpp"
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
    editPlayerButton_ = new QPushButton("Edit Player", central);
    deletePlayerButton_ = new QPushButton("Delete Player", central);
    playerRow->addWidget(playerCombo_, 1);
    playerRow->addWidget(newPlayerName_, 1);
    playerRow->addWidget(addPlayerButton_);
    playerRow->addWidget(editPlayerButton_);
    playerRow->addWidget(deletePlayerButton_);
    rootLayout->addLayout(playerRow);

    characterModel_ = new CharacterTableModel(central);
    characterTable_ = new QTableView(central);
    characterTable_->setModel(characterModel_);
    characterTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    characterTable_->horizontalHeader()->setStretchLastSection(true);
    rootLayout->addWidget(characterTable_, 1);

    auto* characterRow = new QHBoxLayout();
    addCharacterButton_ = new QPushButton("Add Character", central);
    editCharacterButton_ = new QPushButton("Edit Character", central);
    deleteCharacterButton_ = new QPushButton("Delete Character", central);
    characterRow->addWidget(addCharacterButton_);
    characterRow->addWidget(editCharacterButton_);
    characterRow->addWidget(deleteCharacterButton_);
    rootLayout->addLayout(characterRow);

    diceRoller_ = new DiceRollerWidget(central);
    rootLayout->addWidget(diceRoller_);

    setCentralWidget(central);
    resize(640, 480);

    connect(playerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPlayerSelected);
    connect(addPlayerButton_, &QPushButton::clicked, this, &MainWindow::onAddPlayerClicked);
    connect(editPlayerButton_, &QPushButton::clicked, this, &MainWindow::onEditPlayerClicked);
    connect(deletePlayerButton_, &QPushButton::clicked, this, &MainWindow::onDeletePlayerClicked);
    connect(addCharacterButton_, &QPushButton::clicked, this, &MainWindow::onAddCharacterClicked);
    connect(editCharacterButton_, &QPushButton::clicked, this, &MainWindow::onEditCharacterClicked);
    connect(deleteCharacterButton_, &QPushButton::clicked, this, &MainWindow::onDeleteCharacterClicked);
    connect(characterTable_, &QTableView::doubleClicked, this, &MainWindow::onCharacterActivated);

    reloadLookupTables();
    reloadPlayers();
}

int MainWindow::currentPlayerId() const {
    return playerCombo_->currentData().toInt();
}

QString MainWindow::currentPlayerName() const {
    return playerCombo_->currentText();
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

void MainWindow::onEditPlayerClicked() {
    if (playerCombo_->count() == 0) return;

    bool ok = false;
    QString newName = QInputDialog::getText(this, "Edit Player", "Name:", QLineEdit::Normal, currentPlayerName(),
                                             &ok);
    if (!ok) return;
    newName = newName.trimmed();
    if (newName.isEmpty()) return;

    dnd::Player player;
    player.player_id = currentPlayerId();
    player.player_name = newName.toStdString();
    repository_.update_player(player);
    reloadPlayers();
}

void MainWindow::onDeletePlayerClicked() {
    if (playerCombo_->count() == 0) return;

    int player_id = currentPlayerId();
    QString name = currentPlayerName();
    size_t character_count = repository_.get_characters_by_player(player_id).size();

    QString message = character_count > 0
        ? QString("Delete %1? This will also delete %2 character(s).").arg(name).arg(character_count)
        : QString("Delete %1?").arg(name);

    if (QMessageBox::question(this, "Delete Player", message) != QMessageBox::Yes) return;

    repository_.delete_player(player_id);
    reloadPlayers();
}

void MainWindow::onAddCharacterClicked() {
    if (playerCombo_->count() == 0) {
        QMessageBox::information(this, "Add Character", "Add a player first.");
        return;
    }

    CharacterEditorDialog dialog(classes_, races_, alignments_, /*existing=*/nullptr, this);
    if (dialog.exec() == QDialog::Accepted) {
        dnd::Character character = dialog.character();
        character.player_id = currentPlayerId();
        repository_.add_character(character);
        reloadCharactersForCurrentPlayer();
    }
}

void MainWindow::onEditCharacterClicked() {
    QModelIndex index = characterTable_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "Edit Character", "Select a character first.");
        return;
    }

    const dnd::Character& existing = characterModel_->characterAt(index.row());
    CharacterEditorDialog dialog(classes_, races_, alignments_, &existing, this);
    if (dialog.exec() == QDialog::Accepted) {
        dnd::Character updated = dialog.character();
        updated.character_id = existing.character_id;
        updated.player_id = existing.player_id;
        repository_.update_character(updated);
        reloadCharactersForCurrentPlayer();
    }
}

void MainWindow::onDeleteCharacterClicked() {
    QModelIndex index = characterTable_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "Delete Character", "Select a character first.");
        return;
    }

    const dnd::Character& character = characterModel_->characterAt(index.row());
    QString name = QString::fromStdString(character.character_name);
    if (QMessageBox::question(this, "Delete Character", QString("Delete %1?").arg(name)) != QMessageBox::Yes) {
        return;
    }

    repository_.delete_character(character.character_id);
    reloadCharactersForCurrentPlayer();
}

void MainWindow::onCharacterActivated(const QModelIndex& index) {
    if (!index.isValid()) return;

    const dnd::Character& character = characterModel_->characterAt(index.row());
    CharacterSheetDialog dialog(character, classes_, races_, alignments_, this);
    dialog.exec();
}
