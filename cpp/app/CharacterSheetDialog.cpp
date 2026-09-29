#include "CharacterSheetDialog.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "dnd/character_creation.hpp"
#include "dnd/progression.hpp"
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

QString weaponSummaryFor(const dnd::Character& character, const std::vector<dnd::Weapon>& weapons) {
    for (const auto& weapon : weapons) {
        if (weapon.weapon_id != character.weapon_id) continue;

        int bonus = dnd::weapon_attack_bonus(character, weapon);
        int damage_modifier = dnd::weapon_attack_ability_modifier(character, weapon);
        QString damage = QString("%1d%2").arg(weapon.damage_dice_count).arg(weapon.damage_dice_sides);
        if (damage_modifier != 0) damage += formatModifier(damage_modifier);

        return QString("%1 (%2 to hit, %3 damage)")
            .arg(QString::fromStdString(weapon.name), formatModifier(bonus), damage);
    }
    return "None";
}

}  // namespace

CharacterSheetDialog::CharacterSheetDialog(const dnd::Character& character,
                                            const std::vector<dnd::LookupEntry>& classes,
                                            const std::vector<dnd::LookupEntry>& races,
                                            const std::vector<dnd::LookupEntry>& alignments,
                                            const std::vector<dnd::Weapon>& weapons, QWidget* parent)
    : QDialog(parent), dice_(randomSource_) {
    setWindowTitle(QString::fromStdString(character.character_name));

    auto* header = new QLabel(QString("<b>%1</b> — Level %2")
                                   .arg(QString::fromStdString(character.character_name))
                                   .arg(character.level),
                               this);

    auto* rollModeRow = new QHBoxLayout();
    rollModeRow->addWidget(new QLabel("Roll Mode:", this));
    rollModeCombo_ = new QComboBox(this);
    rollModeCombo_->addItem("Normal", static_cast<int>(dnd::RollMode::Normal));
    rollModeCombo_->addItem("Advantage", static_cast<int>(dnd::RollMode::Advantage));
    rollModeCombo_->addItem("Disadvantage", static_cast<int>(dnd::RollMode::Disadvantage));
    rollModeRow->addWidget(rollModeCombo_, 1);

    lastRollLabel_ = new QLabel("Last Roll: -", this);
    lastRollLabel_->setStyleSheet("font-weight: bold;");

    // Adds a row and tooltips both the value label and the row's own name
    // label (found via labelForField) with the same explanation.
    auto addExplainedRow = [](QFormLayout* form, const QString& name, QWidget* value,
                               const QString& explanation) {
        form->addRow(name, value);
        value->setToolTip(explanation);
        if (QWidget* nameLabel = form->labelForField(value)) nameLabel->setToolTip(explanation);
    };

    auto* summaryGroup = new QGroupBox("Summary", this);
    auto* summaryForm = new QFormLayout(summaryGroup);
    summaryForm->addRow("Class", new QLabel(nameFor(classes, character.class_id), summaryGroup));
    summaryForm->addRow("Race", new QLabel(nameFor(races, character.race), summaryGroup));
    summaryForm->addRow("Alignment", new QLabel(nameFor(alignments, character.alignment), summaryGroup));
    if (!character.background.empty()) {
        QString background = QString::fromStdString(character.background);
        if (const dnd::BackgroundInfo* info = dnd::find_background_info(character.background)) {
            addExplainedRow(summaryForm, "Background", new QLabel(background, summaryGroup),
                             QString::fromStdString(info->summary));
        } else {
            summaryForm->addRow("Background", new QLabel(background, summaryGroup));
        }
    }
    addExplainedRow(summaryForm, "Weapon", new QLabel(weaponSummaryFor(character, weapons), summaryGroup),
                     "The weapon this character has equipped, its bonus to hit, and its damage roll.");
    addExplainedRow(summaryForm, "Hit Points",
        new QLabel(QString("%1 / %2").arg(character.current_hp).arg(character.max_hp), summaryGroup),
        "How much damage this character can take before falling unconscious. Current / maximum.");
    QString experienceText = character.level >= dnd::kMaxLevel
        ? QString("%1 (max level)").arg(character.experience)
        : QString("%1 / %2 for level %3")
              .arg(character.experience)
              .arg(dnd::xp_threshold_for_level(character.level + 1))
              .arg(character.level + 1);
    addExplainedRow(summaryForm, "Experience", new QLabel(experienceText, summaryGroup),
        "Earned by defeating monsters, split evenly across the party. Reaching the next threshold levels "
        "this character up, raising max HP and, every few levels, the proficiency bonus.");
    int hitDie = dnd::hit_die_for_class_id(classes, character.class_id);
    addExplainedRow(summaryForm, "Hit Dice",
        new QLabel(QString("%1 / %2 (d%3)").arg(dnd::hit_dice_remaining(character)).arg(character.level).arg(hitDie),
                   summaryGroup),
        "Spent on a short rest to heal (roll the die + Constitution modifier). You have one per level; a long "
        "rest recovers up to half of them.");
    addExplainedRow(summaryForm, "Armor Class",
        new QLabel(QString::number(character.armor_class), summaryGroup),
        "How hard this character is to hit. An attack roll must meet or beat this number to hit.");
    addExplainedRow(summaryForm, "Proficiency Bonus",
        new QLabel(formatModifier(dnd::proficiency_bonus(character.level)), summaryGroup),
        "A bonus added to attack rolls, saving throws, and skill checks this character is proficient in. "
        "It grows as the character levels up.");
    addExplainedRow(summaryForm, "Initiative",
        new QLabel(formatModifier(dnd::initiative(character)), summaryGroup),
        "Added to a d20 roll at the start of combat to determine turn order. Equal to the Dexterity modifier.");
    addExplainedRow(summaryForm, "Passive Perception",
        new QLabel(QString::number(dnd::passive_perception(character)), summaryGroup),
        "A passive score (10 + Perception bonus) used instead of rolling, for noticing things without "
        "actively searching.");

    auto* abilitiesGroup = new QGroupBox("Ability Scores", this);
    auto* abilitiesForm = new QFormLayout(abilitiesGroup);
    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        auto ability = static_cast<dnd::Ability>(i);
        int score = dnd::ability_score(character, ability);
        int modifier = dnd::ability_modifier(score);
        addExplainedRow(abilitiesForm, dnd::ability_name(ability),
            new QLabel(QString("%1 (%2)").arg(score).arg(formatModifier(modifier)), abilitiesGroup),
            dnd::ability_description(ability));
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
        QString explanation = QString("Rolled to resist or shrug off an effect. %1").arg(dnd::ability_description(ability));
        addExplainedRow(savesForm, dnd::ability_name(ability),
            buildRollableRow(savesGroup, name, dnd::saving_throw_bonus(character, ability),
                              dnd::has_save_proficiency(character, ability)),
            explanation);
    }

    auto* skillsGroup = new QGroupBox("Skills", this);
    auto* skillsForm = new QFormLayout(skillsGroup);
    for (int i = 0; i < dnd::kSkillCount; ++i) {
        auto skill = static_cast<dnd::Skill>(i);
        QString name = dnd::skill_name(skill);
        addExplainedRow(skillsForm, name,
            buildRollableRow(skillsGroup, name, dnd::skill_bonus(character, skill),
                              dnd::has_skill_proficiency(character, skill)),
            dnd::skill_description(skill));
    }

    auto* skillsScroll = new QScrollArea(this);
    skillsScroll->setWidget(skillsGroup);
    skillsScroll->setWidgetResizable(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(header);
    layout->addLayout(rollModeRow);
    layout->addWidget(lastRollLabel_);
    layout->addWidget(summaryGroup);
    layout->addWidget(abilitiesGroup);
    layout->addWidget(savesGroup);
    layout->addWidget(skillsScroll, 1);

    resize(380, 640);
}

void CharacterSheetDialog::rollAndShow(const QString& label, int bonus) {
    auto mode = static_cast<dnd::RollMode>(rollModeCombo_->currentData().toInt());
    dnd::D20Roll d20 = dice_.roll_d20(mode);
    int total = d20.result + bonus;

    QString rollsText;
    if (d20.rolls.size() == 1) {
        rollsText = QString("d20(%1)").arg(d20.rolls[0]);
    } else {
        const char* modeTag = mode == dnd::RollMode::Advantage ? "adv" : "dis";
        rollsText = QString("d20(%1, %2 %3)").arg(d20.rolls[0]).arg(d20.rolls[1]).arg(modeTag);
    }

    lastRollLabel_->setText(
        QString("Last Roll: %1 — %2 %3 = %4").arg(label, rollsText, formatModifier(bonus)).arg(total));
}
