#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLabel;
class QListWidget;

// Choosing a subclass (docs/subclasses.md): the class's options with what
// each does through level 5, and the dragon or land when it asks for one.
// Accepting saves the character.
class SubclassDialog : public QDialog {
    Q_OBJECT

public:
    SubclassDialog(dnd::Repository& repository, dnd::Character character, const std::vector<dnd::LookupEntry>& classes,
                   QWidget* parent = nullptr);

    const dnd::Character& character() const { return character_; }

private:
    void showSubclass();
    void choose();

    dnd::Repository& repository_;
    dnd::Character character_;
    std::string className_;
    QListWidget* list_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    QLabel* choiceLabel_ = nullptr;
    QComboBox* choiceCombo_ = nullptr;
    QLabel* errorLabel_ = nullptr;
};
