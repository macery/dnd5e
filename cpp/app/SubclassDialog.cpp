#include "SubclassDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <stdexcept>

#include "WindowSizing.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/subclass.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

}  // namespace

SubclassDialog::SubclassDialog(dnd::Repository& repository, dnd::Character character,
                               const std::vector<dnd::LookupEntry>& classes, QWidget* parent)
    : QDialog(parent), repository_(repository), character_(std::move(character)) {
    for (const auto& entry : classes) {
        if (entry.id == character_.class_id) className_ = entry.name;
    }
    QString kind = q(dnd::subclass_kind(className_));
    setWindowTitle(QString("Choose a %1 — %2").arg(kind, q(character_.character_name)));

    auto* intro = new QLabel(
        QString("%1s choose a %2 at level %3. It shapes what %4 can do from now on. (The subclasses here are the "
                "ones in the free 5e rules reference; features above level 5 are listed but not automated yet.)")
            .arg(q(className_), kind)
            .arg(dnd::subclass_level(className_))
            .arg(q(character_.character_name)),
        this);
    intro->setWordWrap(true);

    list_ = new QListWidget(this);
    for (const auto* info : dnd::subclasses_for(className_)) list_->addItem(info->name);
    list_->setMaximumWidth(220);

    detailLabel_ = new QLabel(this);
    detailLabel_->setWordWrap(true);
    detailLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    choiceLabel_ = new QLabel(this);
    choiceCombo_ = new QComboBox(this);
    auto* choiceRow = new QHBoxLayout();
    choiceRow->addWidget(choiceLabel_);
    choiceRow->addWidget(choiceCombo_, 1);

    auto* right = new QVBoxLayout();
    right->addWidget(detailLabel_, 1);
    right->addLayout(choiceRow);
    auto* columns = new QHBoxLayout();
    columns->addWidget(list_);
    columns->addLayout(right, 1);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Choose");
    connect(buttons, &QDialogButtonBox::accepted, this, &SubclassDialog::choose);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(columns, 1);
    layout->addWidget(errorLabel_);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(640, 420), "Subclass");

    connect(list_, &QListWidget::currentRowChanged, this, [this](int) { showSubclass(); });
    list_->setCurrentRow(0);
    showSubclass();
}

void SubclassDialog::showSubclass() {
    const dnd::SubclassInfo* info = list_->currentItem() ? dnd::find_subclass(list_->currentItem()->text().toStdString())
                                                         : nullptr;
    choiceCombo_->clear();
    if (!info) {
        detailLabel_->setText("No subclass for this class yet.");
        choiceLabel_->hide();
        choiceCombo_->hide();
        return;
    }
    QStringList lines;
    lines << QString("<b>%1</b>: %2").arg(q(info->name), q(info->summary));
    QStringList now;
    QStringList later;
    for (const auto& feature : info->features) {
        QString line = QString("Level %1: %2%3")
                           .arg(feature.level)
                           .arg(feature.name)
                           .arg(feature.automated ? "" : " <i>(not automated yet)</i>");
        (feature.level <= 5 ? now : later) << line;
    }
    lines << "<br><b>Through level 5</b><br>" + now.join("<br>");
    if (!later.isEmpty()) lines << "<br><b>Later</b><br>" + later.join("<br>");
    if (!info->spells.empty()) {
        QStringList spells;
        for (const auto& spell : info->spells) {
            if (*spell.choice) continue;  // land spells: listed by land, below
            spells << QString("%1 (level %2)").arg(spell.spell).arg(spell.class_level);
        }
        if (!spells.isEmpty()) {
            lines << QString("<br><b>%1</b><br>%2")
                         .arg(info->spell_mode == dnd::SubclassSpellMode::AlwaysPrepared
                                  ? "Always-prepared spells (free)"
                                  : "Extra spells you can learn")
                         .arg(spells.join(", "));
        }
    }
    detailLabel_->setText(lines.join("<br>"));

    bool asks = !info->choices.empty();
    choiceLabel_->setVisible(asks);
    choiceCombo_->setVisible(asks);
    if (asks) {
        choiceLabel_->setText(QString(info->choice_label) + ":");
        for (const char* choice : info->choices) choiceCombo_->addItem(choice);
    }
}

void SubclassDialog::choose() {
    if (!list_->currentItem()) return;
    std::string subclass = list_->currentItem()->text().toStdString();
    std::string choice = choiceCombo_->isVisible() ? choiceCombo_->currentText().toStdString() : "";
    try {
        dnd::choose_subclass(character_, className_, subclass, choice);
    } catch (const std::invalid_argument& error) {
        errorLabel_->setText(q(error.what()));
        return;
    }
    repository_.update_character(character_);
    accept();
}
