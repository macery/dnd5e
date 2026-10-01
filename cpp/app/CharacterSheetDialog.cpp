#include "CharacterSheetDialog.hpp"

#include <algorithm>

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "WindowSizing.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/equipment.hpp"
#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

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

// "Longsword: +5 to hit, 1d10+3 slashing (two-handed)".
QString weaponSummaryFor(const dnd::WeaponAttack& attack) {
    QString damage = attack.damage_dice_count > 0
        ? QString("%1d%2").arg(attack.damage_dice_count).arg(attack.damage_dice_sides) +
              (attack.damage_bonus != 0 ? formatModifier(attack.damage_bonus) : QString())
        : QString::number(std::max(0, attack.damage_bonus));
    QString text = QString("%1: %2 to hit, %3 %4")
                       .arg(QString::fromStdString(attack.name), formatModifier(attack.attack_bonus), damage,
                            QString::fromStdString(attack.damage_type));
    if (attack.versatile_two_handed) text += " (two-handed)";
    if (!attack.proficient) text += " — not proficient";
    return text;
}

}  // namespace

CharacterSheetDialog::CharacterSheetDialog(const dnd::Character& character,
                                            const std::vector<dnd::LookupEntry>& classes,
                                            const std::vector<dnd::LookupEntry>& races,
                                            const std::vector<dnd::LookupEntry>& alignments,
                                            const std::vector<dnd::InventoryEntry>& inventory,
                                            const std::vector<dnd::Spell>& spells, QWidget* parent)
    : QDialog(parent), dice_(randomSource_) {
    setWindowTitle(QString::fromStdString(character.character_name));

    auto* header = new QLabel(QString("<b>%1</b> — Level %2%3")
                                   .arg(QString::fromStdString(character.character_name))
                                   .arg(character.level)
                                   .arg(character.dead ? " — <span style='color:#b00020'>DEAD</span>" : ""),
                               this);
    QLabel* deadBanner = nullptr;
    if (character.dead) {
        deadBanner = new QLabel("This character died in battle. Only revival magic (Revivify, Raise Dead, ...) can "
                                "bring them back, and it isn't available yet. They can't fight, rest, or cast.",
                                this);
        deadBanner->setWordWrap(true);
        deadBanner->setStyleSheet("color: #b00020;");
    }

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
        // Long values wrap rather than being cut off in a narrow column.
        if (auto* label = qobject_cast<QLabel*>(value)) label->setWordWrap(true);
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
    std::string className = nameFor(classes, character.class_id).toStdString();
    std::string raceName = nameFor(races, character.race).toStdString();
    auto attack = dnd::weapon_attack(character, className, raceName, inventory);
    addExplainedRow(summaryForm, "Weapon", new QLabel(weaponSummaryFor(attack), summaryGroup),
                     "The weapon this character wields (equip one from the Inventory), its bonus to hit, and its "
                     "damage. Proficiency is added only with weapons the class is trained in; with nothing "
                     "wielded, it's an unarmed strike (1 + STR).");
    addExplainedRow(summaryForm, "Hit Points",
        new QLabel(QString("%1 / %2").arg(character.current_hp).arg(character.max_hp), summaryGroup),
        "How much damage this character can take before falling unconscious. Current / maximum.");
    QString experienceText = character.level >= dnd::kMaxLevel
        ? QString("%1 (max level)").arg(character.experience)
        : QString("%1 / %2 for level %3")
              .arg(character.experience)
              .arg(dnd::xp_threshold_for_level(character.level + 1))
              .arg(character.level + 1);
    if (dnd::pending_levels(character) > 0) {
        experienceText += QString(" — <b>ready to level up</b> (level %1): use Level Up on the main window")
                              .arg(dnd::level_for_xp(character.experience));
    }
    addExplainedRow(summaryForm, "Experience", new QLabel(experienceText, summaryGroup),
        "Earned by defeating monsters, split evenly across the party. Reaching the next threshold makes a "
        "level available: take it with Level Up on the main window (hit points, Ability Score Improvements, "
        "new features and spells).");
    int hitDie = dnd::hit_die_for_class_id(classes, character.class_id);
    addExplainedRow(summaryForm, "Hit Dice",
        new QLabel(QString("%1 / %2 (d%3)").arg(dnd::hit_dice_remaining(character)).arg(character.level).arg(hitDie),
                   summaryGroup),
        "Spent on a short rest to heal (roll the die + Constitution modifier). You have one per level; a long "
        "rest recovers up to half of them.");
    auto armorClass = dnd::calculate_armor_class(character, className, inventory);
    addExplainedRow(summaryForm, "Armor Class",
        new QLabel(QString("%1  (%2)").arg(armorClass.total).arg(QString::fromStdString(armorClass.text())),
                   summaryGroup),
        "How hard this character is to hit. An attack roll must meet or beat this number to hit. Calculated "
        "from worn armor, DEX, a shield, and class features -- equip armor from the Inventory.");
    addExplainedRow(summaryForm, "Speed",
        new QLabel(QString("%1 ft.").arg(dnd::walking_speed(character, raceName, inventory)), summaryGroup),
        "How far this character can move in a turn, from their race. Heavy armor without enough STR costs 10 ft. "
        "(Movement isn't tracked in combat yet.)");
    if (dnd::wearing_armor_without_proficiency(className, inventory)) {
        auto* warning = new QLabel("Wearing armor this class isn't proficient with: disadvantage on STR and DEX "
                                   "attacks, checks, and saves, and no spellcasting.",
                                   summaryGroup);
        warning->setWordWrap(true);
        warning->setStyleSheet("color: #b00020;");
        summaryForm->addRow(warning);
    }
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

    // Class features: limited-use ones with uses left, plus the passive ones.
    QGroupBox* featuresGroup = new QGroupBox("Class Features", this);
    auto* featuresForm = new QFormLayout(featuresGroup);
    for (const auto& info : dnd::limited_features()) {
        if (!dnd::has_feature(info.feature, className, character.level)) continue;
        int max = dnd::feature_max_uses(info.feature, character, className);
        int left = dnd::feature_uses_left(info.feature, character, className);
        bool shortRest = dnd::feature_recovery(info.feature, character.level) == dnd::Recovery::ShortRest;
        QString unit = info.feature == dnd::Feature::LayOnHands ? " HP" : info.feature == dnd::Feature::Ki ? " ki" : "";
        QString uses = max >= 99 ? QString("unlimited")
                                 : QString("%1 / %2%3 (%4 rest)").arg(left).arg(max).arg(unit, shortRest ? "short" : "long");
        addExplainedRow(featuresForm, info.name, new QLabel(uses, featuresGroup), info.description);
    }
    if (className == "Fighter") {
        const dnd::FightingStyleInfo* style = dnd::fighting_style_info(dnd::fighting_style(character, className));
        addExplainedRow(featuresForm, "Fighting Style",
            new QLabel(style ? QString::fromUtf8(style->name) : QString("none -- choose one in Edit Character"),
                       featuresGroup),
            style ? QString::fromUtf8(style->description) : QString("Fighters adopt a Fighting Style at level 1."));
    }
    if (className == "Barbarian" && character.level >= 2) {
        addExplainedRow(featuresForm, "Reckless Attack", new QLabel("advantage on STR melee attacks", featuresGroup),
            "On your first attack of a turn you can attack recklessly: advantage on STR melee weapon attacks this "
            "turn, but attacks against you have advantage until your next turn.");
    }
    if (className == "Paladin" && character.level >= 2) {
        addExplainedRow(featuresForm, "Divine Smite", new QLabel("+2d8 radiant per slot, +1d8 per level above 1st",
                                                                 featuresGroup),
            "When you hit with a melee weapon attack, spend a spell slot for extra radiant damage: 2d8 with a "
            "1st-level slot, +1d8 for each level above 1st, to a maximum of 5d8.");
    }
    if (className == "Barbarian") {
        addExplainedRow(featuresForm, "Rage Damage",
            new QLabel(QString("+%1").arg(dnd::rage_damage_bonus(character.level)), featuresGroup),
            "Added to damage with STR melee weapon attacks while raging.");
    }
    if (className == "Rogue") {
        addExplainedRow(featuresForm, "Sneak Attack",
            new QLabel(QString("%1d6").arg(dnd::sneak_attack_dice(character.level)), featuresGroup),
            "Once per turn, extra damage on a hit with a finesse or ranged weapon, if you have advantage -- or an "
            "ally is also fighting the target -- and you don't have disadvantage.");
    }
    if (className == "Rogue" && character.level >= 2) {
        addExplainedRow(featuresForm, "Cunning Action", new QLabel("Dash or Disengage as a bonus action", featuresGroup),
            "Your quick thinking lets you Dash or Disengage as a bonus action. (Movement isn't tracked yet.)");
    }
    if (className == "Monk") {
        addExplainedRow(featuresForm, "Martial Arts",
            new QLabel(QString("d%1 unarmed, DEX, bonus unarmed strike").arg(dnd::martial_arts_die(character.level)),
                       featuresGroup),
            "Without armor or a shield: unarmed strikes and monk weapons can use DEX and the Martial Arts die, and "
            "after the Attack action with one you can make an unarmed strike as a bonus action.");
    }
    int attacks = dnd::attacks_per_attack_action(className, character.level);
    if (attacks > 1) {
        addExplainedRow(featuresForm, "Extra Attack", new QLabel(QString("%1 attacks per Attack action").arg(attacks),
                                                                 featuresGroup),
            "When you take the Attack action, you attack more than once.");
    }
    if (featuresForm->rowCount() == 0) {
        delete featuresGroup;
        featuresGroup = nullptr;
    }

    QGroupBox* spellsGroup = nullptr;
    const dnd::ClassInfo* classInfo = dnd::find_class_info_by_id(classes, character.class_id);
    if (classInfo && dnd::is_spellcaster(*classInfo)) {
        spellsGroup = new QGroupBox("Spellcasting", this);
        auto* spellsForm = new QFormLayout(spellsGroup);
        addExplainedRow(spellsForm, "Ability",
            new QLabel(dnd::ability_name(classInfo->spellcasting.ability), spellsGroup),
            "The ability this class casts spells with.");
        addExplainedRow(spellsForm, "Spell Attack",
            new QLabel(formatModifier(dnd::spell_attack_bonus(character, *classInfo)), spellsGroup),
            "Added to a d20 for spells that make an attack roll: proficiency bonus + spellcasting modifier.");
        addExplainedRow(spellsForm, "Spell Save DC",
            new QLabel(QString::number(dnd::spell_save_dc(character, *classInfo)), spellsGroup),
            "What a target must meet or beat on its saving throw to resist your spell: 8 + proficiency bonus + "
            "spellcasting modifier.");

        QStringList slotParts;
        auto totals = dnd::spell_slots(*classInfo, character.level);
        for (int level = 1; level <= dnd::kMaxSpellLevel; ++level) {
            int total = totals[static_cast<size_t>(level - 1)];
            if (total > 0) {
                slotParts << QString("L%1: %2/%3")
                                 .arg(level)
                                 .arg(dnd::slots_remaining(character, *classInfo, level))
                                 .arg(total);
            }
        }
        addExplainedRow(spellsForm, "Spell Slots",
            new QLabel(slotParts.isEmpty() ? "none yet" : slotParts.join(" · "), spellsGroup),
            "Remaining / total slots per spell level. A long rest restores them (Warlocks: a short rest too).");

        QStringList names;
        for (const auto& spell : spells) names << QString::fromStdString(spell.name);
        auto* spellList = new QLabel(names.isEmpty() ? "none -- use the Spellbook to learn some" : names.join(", "),
                                     spellsGroup);
        spellList->setWordWrap(true);
        addExplainedRow(spellsForm, "Spells", spellList,
            "Open the Spellbook from the main window for details, to learn spells, or to cast outside combat.");
    }

    // The sections in two columns when there's room, one scrolling column when not.
    auto* sections = new ReflowColumns(620, this);
    sections->addWidget(0, summaryGroup);
    sections->addWidget(0, abilitiesGroup);
    sections->addWidget(0, savesGroup);
    if (featuresGroup) sections->addWidget(1, featuresGroup);
    if (spellsGroup) sections->addWidget(1, spellsGroup);
    sections->addWidget(1, skillsGroup);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(header);
    if (deadBanner) layout->addWidget(deadBanner);
    layout->addLayout(rollModeRow);
    layout->addWidget(lastRollLabel_);
    layout->addWidget(scrollable(sections, this), 1);

    fitToScreen(this, QSize(820, 760), "CharacterSheet");
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
