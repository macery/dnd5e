#include "CharacterSheetDialog.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "dnd/rules.hpp"

namespace {

QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}

QString nameFor(const std::vector<dnd::LookupEntry>& entries, int id) {
    for (const auto& entry : entries) {
        if (entry.id == id) return QString::fromStdString(entry.name);
    }
    return QString("#%1").arg(id);
}

}  // namespace

CharacterSheetDialog::CharacterSheetDialog(const dnd::Character& character,
                                            const std::vector<dnd::LookupEntry>& classes,
                                            const std::vector<dnd::LookupEntry>& races,
                                            const std::vector<dnd::LookupEntry>& alignments, QWidget* parent)
    : QDialog(parent), dice_(randomSource_) {
    setWindowTitle(QString::fromStdString(character.character_name));

    auto* header = new QLabel(QString("<b>%1</b> — Level %2")
                                   .arg(QString::fromStdString(character.character_name))
                                   .arg(character.level),
                               this);

    lastRollLabel_ = new QLabel("Last Roll: -", this);
    lastRollLabel_->setStyleSheet("font-weight: bold;");

    auto* summaryGroup = new QGroupBox("Summary", this);
    auto* summaryForm = new QFormLayout(summaryGroup);
    summaryForm->addRow("Class", new QLabel(nameFor(classes, character.class_id), summaryGroup));
    summaryForm->addRow("Race", new QLabel(nameFor(races, character.race), summaryGroup));
    summaryForm->addRow("Alignment", new QLabel(nameFor(alignments, character.alignment), summaryGroup));
    summaryForm->addRow("Hit Points",
        new QLabel(QString("%1 / %2").arg(character.current_hp).arg(character.max_hp), summaryGroup));
    summaryForm->addRow("Armor Class", new QLabel(QString::number(character.armor_class), summaryGroup));
    summaryForm->addRow("Proficiency Bonus",
        new QLabel(formatModifier(dnd::proficiency_bonus(character.level)), summaryGroup));
    summaryForm->addRow("Initiative", new QLabel(formatModifier(dnd::initiative(character)), summaryGroup));
    summaryForm->addRow("Passive Perception",
        new QLabel(QString::number(dnd::passive_perception(character)), summaryGroup));

    auto* abilitiesGroup = new QGroupBox("Ability Scores", this);
    auto* abilitiesForm = new QFormLayout(abilitiesGroup);
    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        auto ability = static_cast<dnd::Ability>(i);
        int score = dnd::ability_score(character, ability);
        int modifier = dnd::ability_modifier(score);
        abilitiesForm->addRow(
            dnd::ability_name(ability),
            new QLabel(QString("%1 (%2)").arg(score).arg(formatModifier(modifier)), abilitiesGroup));
    }

    // Each saving throw / skill row is a bonus label plus a "Roll" button
    // that rolls a d20 against that specific bonus.
    auto buildRollableRow = [this](QWidget* rowParent, const QString& rollName, int bonus,
                                    bool proficient) -> QWidget* {
        auto* row = new QWidget(rowParent);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        QString text = formatModifier(bonus);
        if (proficient) text += "  (proficient)";
        rowLayout->addWidget(new QLabel(text, row), 1);

        auto* rollButton = new QPushButton("Roll", row);
        connect(rollButton, &QPushButton::clicked, this,
                [this, rollName, bonus]() { rollAndShow(rollName, bonus); });
        rowLayout->addWidget(rollButton);

        return row;
    };

    auto* savesGroup = new QGroupBox("Saving Throws", this);
    auto* savesForm = new QFormLayout(savesGroup);
    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        auto ability = static_cast<dnd::Ability>(i);
        QString name = QString(dnd::ability_name(ability)) + " Save";
        savesForm->addRow(dnd::ability_name(ability),
            buildRollableRow(savesGroup, name, dnd::saving_throw_bonus(character, ability),
                              dnd::has_save_proficiency(character, ability)));
    }

    auto* skillsGroup = new QGroupBox("Skills", this);
    auto* skillsForm = new QFormLayout(skillsGroup);
    for (int i = 0; i < dnd::kSkillCount; ++i) {
        auto skill = static_cast<dnd::Skill>(i);
        QString name = dnd::skill_name(skill);
        skillsForm->addRow(name,
            buildRollableRow(skillsGroup, name, dnd::skill_bonus(character, skill),
                              dnd::has_skill_proficiency(character, skill)));
    }

    auto* skillsScroll = new QScrollArea(this);
    skillsScroll->setWidget(skillsGroup);
    skillsScroll->setWidgetResizable(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(header);
    layout->addWidget(lastRollLabel_);
    layout->addWidget(summaryGroup);
    layout->addWidget(abilitiesGroup);
    layout->addWidget(savesGroup);
    layout->addWidget(skillsScroll, 1);

    resize(380, 640);
}

void CharacterSheetDialog::rollAndShow(const QString& label, int bonus) {
    int roll = dice_.roll(20, 1).front();
    int total = roll + bonus;
    lastRollLabel_->setText(
        QString("Last Roll: %1 — d20(%2) %3 = %4").arg(label).arg(roll).arg(formatModifier(bonus)).arg(total));
}
