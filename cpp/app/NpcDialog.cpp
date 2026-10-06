#include "NpcDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "WindowSizing.hpp"
#include "dnd/campaign.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

constexpr dnd::Attitude kAttitudes[] = {dnd::Attitude::Friendly, dnd::Attitude::Indifferent, dnd::Attitude::Hostile};

}  // namespace

NpcDialog::NpcDialog(dnd::Repository& repository, QWidget* parent)
    : QDialog(parent), repository_(repository), campaign_id_(repository.active_campaign_id()) {
    std::string campaign = dnd::active_campaign_name(repository_);
    setWindowTitle(campaign.empty() ? QString("NPCs") : QString("NPCs — %1").arg(q(campaign)));
    // The Shop's and the Temple's are always there to edit.
    dnd::shop_merchant(repository_, campaign_id_);
    dnd::temple_priest(repository_, campaign_id_);

    auto* intro = new QLabel(
        QString("The people the party deals with%1. Their attitude sets how hard they are to talk round (DMG ch. 8), "
                "and it lasts: whoever the party helps or crosses remembers it in later adventures. Knowing what "
                "they want and fear makes every request easier.")
            .arg(campaign.empty() ? QString(" outside any campaign") : QString(" in %1").arg(q(campaign))),
        this);
    intro->setWordWrap(true);

    npcList_ = new QListWidget(this);
    auto* newButton = new QPushButton("New NPC", this);
    deleteButton_ = new QPushButton("Delete", this);
    deleteButton_->setToolTip("An adventure's NPC comes back as it first was, the next time the party meets them.");
    auto* listButtons = new QHBoxLayout();
    listButtons->addWidget(newButton);
    listButtons->addWidget(deleteButton_);
    auto* left = new QVBoxLayout();
    left->addWidget(npcList_, 1);
    left->addLayout(listButtons);

    nameEdit_ = new QLineEdit(this);
    roleEdit_ = new QLineEdit(this);
    roleEdit_->setPlaceholderText("innkeeper, cult priest, ...");
    attitudeCombo_ = new QComboBox(this);
    for (auto attitude : kAttitudes) attitudeCombo_->addItem(dnd::attitude_name(attitude), static_cast<int>(attitude));
    attitudeCombo_->setToolTip("Friendly NPCs help at no risk without a check; Indifferent ones need DC 10 to help "
                               "at all; Hostile ones need DC 10 just to stop opposing you.");
    descriptionEdit_ = new QPlainTextEdit(this);
    descriptionEdit_->setMaximumHeight(70);
    wantsEdit_ = new QLineEdit(this);
    fearsEdit_ = new QLineEdit(this);
    knownBox_ = new QCheckBox("The party knows what they want and fear (DCs 5 lower)", this);
    statBlockCombo_ = new QComboBox(this);
    statBlockCombo_->addItem("None", "");
    for (const auto& monster : repository_.get_monsters()) {
        statBlockCombo_->addItem(q(monster.name), q(monster.name));
    }
    statBlockCombo_->setToolTip("The stat block they'd fight with, from the monster list. NPCs join fights in a "
                                "later phase.");
    saveButton_ = new QPushButton("Save", this);
    auto* form = new QFormLayout();
    form->addRow("Name", nameEdit_);
    form->addRow("Role", roleEdit_);
    form->addRow("Attitude", attitudeCombo_);
    form->addRow("Description", descriptionEdit_);
    form->addRow("Wants", wantsEdit_);
    form->addRow("Fears", fearsEdit_);
    form->addRow("", knownBox_);
    form->addRow("Stat block", statBlockCombo_);
    auto* saveRow = new QHBoxLayout();
    saveRow->addStretch(1);
    saveRow->addWidget(saveButton_);
    auto* right = new QVBoxLayout();
    right->addLayout(form, 1);
    right->addLayout(saveRow);

    auto* columns = new QHBoxLayout();
    columns->addLayout(left, 2);
    columns->addLayout(right, 3);

    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->addWidget(intro);
    contentLayout->addLayout(columns, 1);
    contentLayout->addWidget(messageLabel_);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scrollable(content, this), 1);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(680, 520), "NPCs");

    connect(npcList_, &QListWidget::currentRowChanged, this, [this](int) { showNpc(); });
    connect(newButton, &QPushButton::clicked, this, &NpcDialog::newNpc);
    connect(saveButton_, &QPushButton::clicked, this, &NpcDialog::saveNpc);
    connect(deleteButton_, &QPushButton::clicked, this, &NpcDialog::deleteNpc);

    reloadNpcs(0);
}

int NpcDialog::selectedId() const {
    int row = npcList_->currentRow();
    return row >= 0 && row < static_cast<int>(npcs_.size()) ? npcs_[static_cast<size_t>(row)].npc_id : 0;
}

void NpcDialog::reloadNpcs(int select_id) {
    npcs_ = repository_.get_npcs(campaign_id_);
    npcList_->blockSignals(true);
    npcList_->clear();
    int selectRow = npcs_.empty() ? -1 : 0;
    for (int row = 0; row < static_cast<int>(npcs_.size()); ++row) {
        const auto& npc = npcs_[static_cast<size_t>(row)];
        QString label = q(npc.name);
        if (!npc.role.empty()) label += QString(", %1").arg(q(npc.role));
        npcList_->addItem(QString("%1 (%2)").arg(label, dnd::attitude_name(npc.attitude)));
        if (npc.npc_id == select_id) selectRow = row;
    }
    npcList_->setCurrentRow(selectRow);
    npcList_->blockSignals(false);
    showNpc();
}

void NpcDialog::showNpc() {
    int id = selectedId();
    bool any = id != 0;
    for (QWidget* w : {static_cast<QWidget*>(nameEdit_), static_cast<QWidget*>(roleEdit_),
                       static_cast<QWidget*>(attitudeCombo_), static_cast<QWidget*>(descriptionEdit_),
                       static_cast<QWidget*>(wantsEdit_), static_cast<QWidget*>(fearsEdit_),
                       static_cast<QWidget*>(knownBox_), static_cast<QWidget*>(statBlockCombo_),
                       static_cast<QWidget*>(saveButton_), static_cast<QWidget*>(deleteButton_)}) {
        w->setEnabled(any);
    }
    if (!any) return;
    const auto& npc = npcs_[static_cast<size_t>(npcList_->currentRow())];
    nameEdit_->setText(q(npc.name));
    roleEdit_->setText(q(npc.role));
    attitudeCombo_->setCurrentIndex(attitudeCombo_->findData(static_cast<int>(npc.attitude)));
    descriptionEdit_->setPlainText(q(npc.description));
    wantsEdit_->setText(q(npc.wants));
    fearsEdit_->setText(q(npc.fears));
    knownBox_->setChecked(npc.known);
    statBlockCombo_->setCurrentIndex(std::max(0, statBlockCombo_->findData(q(npc.stat_block))));
    // The Shop's and the Temple's can't go: the counter and the altar need someone.
    bool fixture = npc.key == "merchant" || npc.key == "priest";
    deleteButton_->setEnabled(!fixture);
}

void NpcDialog::newNpc() {
    dnd::Npc npc;
    npc.campaign_id = campaign_id_;
    npc.name = "New NPC";
    int id = repository_.add_npc(npc);
    messageLabel_->setText("Created. Fill them in and Save.");
    reloadNpcs(id);
    nameEdit_->setFocus();
    nameEdit_->selectAll();
}

void NpcDialog::saveNpc() {
    int id = selectedId();
    if (id == 0) return;
    QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) {
        messageLabel_->setText("An NPC needs a name.");
        return;
    }
    dnd::Npc npc = npcs_[static_cast<size_t>(npcList_->currentRow())];
    npc.name = name.toStdString();
    npc.role = roleEdit_->text().trimmed().toStdString();
    npc.attitude = static_cast<dnd::Attitude>(attitudeCombo_->currentData().toInt());
    npc.description = descriptionEdit_->toPlainText().trimmed().toStdString();
    npc.wants = wantsEdit_->text().trimmed().toStdString();
    npc.fears = fearsEdit_->text().trimmed().toStdString();
    npc.known = knownBox_->isChecked();
    npc.stat_block = statBlockCombo_->currentData().toString().toStdString();
    repository_.update_npc(npc);
    messageLabel_->setText(QString("Saved %1.").arg(name));
    reloadNpcs(id);
}

void NpcDialog::deleteNpc() {
    int id = selectedId();
    if (id == 0) return;
    QString name = q(npcs_[static_cast<size_t>(npcList_->currentRow())].name);
    repository_.delete_npc(id);
    messageLabel_->setText(QString("Deleted %1.").arg(name));
    reloadNpcs(0);
}
