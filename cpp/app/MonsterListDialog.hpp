#pragma once

#include <QDialog>
#include <vector>

#include "dnd/monster.hpp"

// Read-only reference viewer for the seeded sample enemies -- static data,
// no editing, no ongoing data flow, so a plain QTableWidget is populated
// once rather than a full QAbstractTableModel.
class MonsterListDialog : public QDialog {
    Q_OBJECT

public:
    explicit MonsterListDialog(std::vector<dnd::Monster> monsters, QWidget* parent = nullptr);
};
