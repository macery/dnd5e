#pragma once

#include <QDialog>

class QLabel;
class QListWidget;

// Lets the player pick one of the pre-built dnd::AdventureTemplate entries
// for a guided solo encounter: a list of titles on the left, the selected
// adventure's hook text on the right.
class AdventurePickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit AdventurePickerDialog(QWidget* parent = nullptr);

    // Index into dnd::adventure_templates(). Only meaningful when exec()
    // returned QDialog::Accepted.
    int selectedIndex() const;

private slots:
    void onSelectionChanged(int index);

private:
    QListWidget* list_ = nullptr;
    QLabel* hookLabel_ = nullptr;
};
