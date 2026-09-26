#pragma once

#include <QWidget>

#include "dnd/dice.hpp"

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;

// Self-contained: talks directly to dnd::Dice, no dependency on Repository.
class DiceRollerWidget : public QWidget {
    Q_OBJECT

public:
    explicit DiceRollerWidget(QWidget* parent = nullptr);

private slots:
    void onRollClicked();

private:
    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_;

    QComboBox* sidesCombo_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QPushButton* rollButton_ = nullptr;
    QLabel* resultLabel_ = nullptr;
};
