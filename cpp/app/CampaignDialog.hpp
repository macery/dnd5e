#pragma once

#include <QDialog>
#include <vector>

#include "dnd/repository.hpp"

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

// Create, rename, and delete campaigns, and choose each one's roster from
// every player's characters (docs/campaigns.md). A character plays in at
// most one campaign: checking them here moves them from any other.
class CampaignDialog : public QDialog {
    Q_OBJECT

public:
    explicit CampaignDialog(dnd::Repository& repository, QWidget* parent = nullptr);

private:
    void reloadCampaigns(int select_id);
    void showCampaign();
    void newCampaign();
    void saveCampaign();
    void deleteCampaign();
    int selectedId() const;

    dnd::Repository& repository_;
    std::vector<dnd::Campaign> campaigns_;
    QListWidget* campaignList_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QPlainTextEdit* notesEdit_ = nullptr;
    QListWidget* roster_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QLabel* messageLabel_ = nullptr;
};
