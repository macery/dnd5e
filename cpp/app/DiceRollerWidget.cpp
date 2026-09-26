#include "DiceRollerWidget.hpp"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

DiceRollerWidget::DiceRollerWidget(QWidget* parent) : QWidget(parent), dice_(randomSource_) {
    auto* group = new QGroupBox("Dice Roller", this);
    auto* groupLayout = new QHBoxLayout(group);

    sidesCombo_ = new QComboBox(group);
    for (int sides : {4, 6, 8, 10, 12, 20, 100}) {
        sidesCombo_->addItem(QString("d%1").arg(sides), sides);
    }

    countSpin_ = new QSpinBox(group);
    countSpin_->setRange(1, 20);
    countSpin_->setValue(1);

    rollButton_ = new QPushButton("Roll", group);
    resultLabel_ = new QLabel("-", group);

    groupLayout->addWidget(sidesCombo_);
    groupLayout->addWidget(countSpin_);
    groupLayout->addWidget(rollButton_);
    groupLayout->addWidget(resultLabel_, 1);

    auto* outer = new QHBoxLayout(this);
    outer->addWidget(group);

    connect(rollButton_, &QPushButton::clicked, this, &DiceRollerWidget::onRollClicked);
}

void DiceRollerWidget::onRollClicked() {
    int sides = sidesCombo_->currentData().toInt();
    int count = countSpin_->value();
    auto rolls = dice_.roll(sides, count);

    QStringList parts;
    for (int roll : rolls) parts << QString::number(roll);

    resultLabel_->setText(QString("[%1] = %2").arg(parts.join(", ")).arg(dnd::Dice::sum(rolls)));
}
