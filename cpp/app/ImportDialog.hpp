#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character_io.hpp"
#include "dnd/repository.hpp"

class QComboBox;
class QLabel;
class QTableWidget;

// The preview before importing a character file (docs/save-load.md): which
// player the characters go to, each character with what will happen to it
// (imported, imported as a copy / replace / skip on a name conflict, or
// refused with the reason), and warnings for anything that won't come
// across. Import applies it with dnd::apply_import; nothing changes before.
class ImportDialog : public QDialog {
    Q_OBJECT

public:
    ImportDialog(dnd::Repository& repository, dnd::CharacterExport file, QWidget* parent = nullptr);

    // After Import: what happened (one line per character) and where.
    const dnd::ImportResult& result() const { return result_; }

    void accept() override;

private:
    dnd::Repository& repository_;
    dnd::ImportPlan plan_;
    dnd::ImportResult result_;
    QComboBox* playerCombo_ = nullptr;           // data: player id, 0 = the file's player
    std::vector<QComboBox*> conflictCombos_;     // per plan character; nullptr if no conflict
    QTableWidget* table_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
};
