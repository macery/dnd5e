#include "ClassOptionsDialog.hpp"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "WindowSizing.hpp"
#include "dnd/spellbook.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

}  // namespace

dnd::SpellContext ClassOptionsDialog::spellContext(const dnd::Repository& repository, const dnd::Character& character) {
    dnd::SpellContext context;
    for (const auto& spell : repository.get_spells()) {
        if (spell.level == 0) context.cantrips.push_back(spell.name);
    }
    for (const auto& spell : dnd::spells_ready(repository, character)) context.known.push_back(spell.name);
    return context;
}

ClassOptionsDialog::ClassOptionsDialog(dnd::Repository& repository, dnd::Character character,
                                       const std::vector<dnd::LookupEntry>& classes, QWidget* parent)
    : QDialog(parent), repository_(repository), character_(std::move(character)) {
    for (const auto& entry : classes) {
        if (entry.id == character_.class_id) className_ = entry.name;
    }
    setWindowTitle(QString("Class Options — %1").arg(q(character_.character_name)));
    spells_ = spellContext(repository_, character_);
    choices_ = dnd::class_choices_due(character_, className_, spells_);

    auto* content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(content);
    auto* intro = new QLabel(QString("%1 has choices to make as a level-%2 %3.")
                                 .arg(q(character_.character_name))
                                 .arg(character_.level)
                                 .arg(q(className_)),
                             content);
    intro->setWordWrap(true);
    contentLayout->addWidget(intro);
    for (const auto& choice : choices_) {
        auto* group = new QGroupBox(QString("%1: choose %2").arg(q(choice.label)).arg(choice.count), content);
        auto* groupLayout = new QVBoxLayout(group);
        auto* list = new QListWidget(group);
        for (size_t i = 0; i < choice.options.size(); ++i) {
            auto* item = new QListWidgetItem(q(choice.options[i]), list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            if (i < choice.descriptions.size() && !choice.descriptions[i].empty()) {
                item->setToolTip(q(choice.descriptions[i]));
            }
        }
        list->setMinimumHeight(80);
        groupLayout->addWidget(list);
        contentLayout->addWidget(group);
        lists_.push_back(list);
    }
    if (choices_.empty()) contentLayout->addWidget(new QLabel("Nothing to choose right now.", content));

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet("color: #b00020;");
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Choose");
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!choices_.empty());
    connect(buttons, &QDialogButtonBox::accepted, this, &ClassOptionsDialog::choose);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scrollable(content, this), 1);
    layout->addWidget(errorLabel_);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(520, 560), "ClassOptions");
}

void ClassOptionsDialog::choose() {
    dnd::Character updated = character_;
    // Pact Boon first: the invocations it unlocks (Thirsting Blade) are checked against it.
    std::vector<size_t> order;
    for (size_t i = 0; i < choices_.size(); ++i) {
        if (choices_[i].key == "PactBoon") order.insert(order.begin(), i);
        else order.push_back(i);
    }
    for (size_t i : order) {
        std::vector<std::string> picks;
        for (int row = 0; row < lists_[i]->count(); ++row) {
            if (lists_[i]->item(row)->checkState() == Qt::Checked) picks.push_back(lists_[i]->item(row)->text().toStdString());
        }
        std::string error = dnd::apply_class_choice(updated, className_, choices_[i].key, picks, spells_);
        if (!error.empty()) {
            errorLabel_->setText(q(error));
            return;
        }
    }
    repository_.update_character(updated);
    character_ = updated;
    accept();
}
