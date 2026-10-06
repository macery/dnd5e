#include "CampaignDialog.hpp"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <exception>

#include "WindowSizing.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

}  // namespace

CampaignDialog::CampaignDialog(dnd::Repository& repository, QWidget* parent)
    : QDialog(parent), repository_(repository) {
    setWindowTitle("Campaigns");

    auto* intro = new QLabel(
        "A campaign is a table that plays together: characters from any players. While one is active (choose it on "
        "the main window), fights, adventure parties, trading, and the Temple use only its characters, and its "
        "adventures resume separately. A character plays in one campaign at a time.",
        this);
    intro->setWordWrap(true);

    campaignList_ = new QListWidget(this);
    auto* newButton = new QPushButton("New Campaign", this);
    deleteButton_ = new QPushButton("Delete", this);
    deleteButton_->setToolTip("The campaign and its NPCs go; its characters and adventure runs stay, outside any "
                              "campaign.");
    auto* listButtons = new QHBoxLayout();
    listButtons->addWidget(newButton);
    listButtons->addWidget(deleteButton_);
    auto* left = new QVBoxLayout();
    left->addWidget(campaignList_, 1);
    left->addLayout(listButtons);

    nameEdit_ = new QLineEdit(this);
    notesEdit_ = new QPlainTextEdit(this);
    notesEdit_->setPlaceholderText("Notes: where the party is, what happened last session, ...");
    notesEdit_->setMaximumHeight(90);
    roster_ = new QListWidget(this);
    roster_->setToolTip("Check the characters who play in this campaign.");
    saveButton_ = new QPushButton("Save", this);
    auto* form = new QFormLayout();
    form->addRow("Name", nameEdit_);
    form->addRow("Notes", notesEdit_);
    form->addRow("Roster", roster_);
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

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(columns, 1);
    layout->addWidget(messageLabel_);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(640, 480), "Campaigns");

    connect(campaignList_, &QListWidget::currentRowChanged, this, [this](int) { showCampaign(); });
    connect(newButton, &QPushButton::clicked, this, &CampaignDialog::newCampaign);
    connect(saveButton_, &QPushButton::clicked, this, &CampaignDialog::saveCampaign);
    connect(deleteButton_, &QPushButton::clicked, this, &CampaignDialog::deleteCampaign);

    reloadCampaigns(repository_.active_campaign_id());
}

int CampaignDialog::selectedId() const {
    int row = campaignList_->currentRow();
    return row >= 0 && row < static_cast<int>(campaigns_.size()) ? campaigns_[static_cast<size_t>(row)].campaign_id
                                                                  : 0;
}

void CampaignDialog::reloadCampaigns(int select_id) {
    campaigns_ = repository_.get_campaigns();
    campaignList_->blockSignals(true);
    campaignList_->clear();
    int selectRow = campaigns_.empty() ? -1 : 0;
    for (int row = 0; row < static_cast<int>(campaigns_.size()); ++row) {
        const auto& campaign = campaigns_[static_cast<size_t>(row)];
        campaignList_->addItem(q(campaign.name) + (campaign.active ? "  (active)" : ""));
        if (campaign.campaign_id == select_id) selectRow = row;
    }
    campaignList_->setCurrentRow(selectRow);
    campaignList_->blockSignals(false);
    showCampaign();
}

void CampaignDialog::showCampaign() {
    int id = selectedId();
    bool any = id != 0;
    for (QWidget* w : {static_cast<QWidget*>(nameEdit_), static_cast<QWidget*>(notesEdit_),
                       static_cast<QWidget*>(roster_), static_cast<QWidget*>(saveButton_),
                       static_cast<QWidget*>(deleteButton_)}) {
        w->setEnabled(any);
    }
    nameEdit_->clear();
    notesEdit_->clear();
    roster_->clear();
    if (!any) return;
    const auto& campaign = campaigns_[static_cast<size_t>(campaignList_->currentRow())];
    nameEdit_->setText(q(campaign.name));
    notesEdit_->setPlainText(q(campaign.notes));

    auto players = repository_.get_players();
    for (const auto& character : repository_.get_characters()) {
        QString player;
        for (const auto& p : players) {
            if (p.player_id == character.player_id) player = q(p.player_name);
        }
        QString label = QString("%1 (%2), level %3").arg(q(character.character_name), player).arg(character.level);
        for (const auto& other : campaigns_) {
            if (other.campaign_id == character.campaign_id && other.campaign_id != id) {
                label += QString(" — in %1").arg(q(other.name));
            }
        }
        if (character.dead) label += " — dead";
        auto* item = new QListWidgetItem(label, roster_);
        item->setData(Qt::UserRole, character.character_id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(character.campaign_id == id ? Qt::Checked : Qt::Unchecked);
    }
}

void CampaignDialog::newCampaign() {
    // "New Campaign", "New Campaign 2", ...: names are unique.
    QString name = "New Campaign";
    for (int n = 2;; ++n) {
        bool taken = false;
        for (const auto& c : campaigns_) taken = taken || q(c.name) == name;
        if (!taken) break;
        name = QString("New Campaign %1").arg(n);
    }
    dnd::Campaign campaign;
    campaign.name = name.toStdString();
    campaign.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    int id = repository_.add_campaign(campaign);
    messageLabel_->setText("Created. Name it, check its characters, and Save.");
    reloadCampaigns(id);
    nameEdit_->setFocus();
    nameEdit_->selectAll();
}

void CampaignDialog::saveCampaign() {
    int id = selectedId();
    if (id == 0) return;
    QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) {
        messageLabel_->setText("A campaign needs a name.");
        return;
    }
    dnd::Campaign campaign = campaigns_[static_cast<size_t>(campaignList_->currentRow())];
    campaign.name = name.toStdString();
    campaign.notes = notesEdit_->toPlainText().toStdString();
    try {
        repository_.with_transaction([&] {
            repository_.update_campaign(campaign);
            for (int row = 0; row < roster_->count(); ++row) {
                auto* item = roster_->item(row);
                int character_id = item->data(Qt::UserRole).toInt();
                bool member = item->checkState() == Qt::Checked;
                for (const auto& character : repository_.get_characters()) {
                    if (character.character_id != character_id) continue;
                    if (member && character.campaign_id != id) repository_.set_character_campaign(character_id, id);
                    if (!member && character.campaign_id == id) repository_.set_character_campaign(character_id, 0);
                }
            }
        });
    } catch (const std::exception&) {
        messageLabel_->setText(QString("There's already a campaign called \"%1\".").arg(name));
        return;
    }
    messageLabel_->setText(QString("Saved %1.").arg(name));
    reloadCampaigns(id);
}

void CampaignDialog::deleteCampaign() {
    int id = selectedId();
    if (id == 0) return;
    QString name = q(campaigns_[static_cast<size_t>(campaignList_->currentRow())].name);
    repository_.delete_campaign(id);
    messageLabel_->setText(
        QString("Deleted %1 and its NPCs. Its characters are still here, outside any campaign.").arg(name));
    reloadCampaigns(0);
}
