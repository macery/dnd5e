#include "CharacterEditorDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
void populateLookupCombo(QComboBox* combo, const std::vector<dnd::LookupEntry>& entries) {
    for (const auto& entry : entries) {
        combo->addItem(QString::fromStdString(entry.name), entry.id);
    }
}
}  // namespace

CharacterEditorDialog::CharacterEditorDialog(const std::vector<dnd::LookupEntry>& classes,
                                              const std::vector<dnd::LookupEntry>& races,
                                              const std::vector<dnd::LookupEntry>& alignments,
                                              const std::vector<dnd::Weapon>& weapons,
                                              const dnd::Character* existing, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(existing ? "Edit Character" : "Add Character");

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildBasicsTab(classes, races, alignments, weapons), "Basics");
    tabs->addTab(buildAbilityScoresTab(), "Ability Scores");
    tabs->addTab(buildProficienciesTab(), "Proficiencies");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
    resize(420, 360);

    if (existing) populateFrom(*existing);
}

QWidget* CharacterEditorDialog::buildBasicsTab(const std::vector<dnd::LookupEntry>& classes,
                                                const std::vector<dnd::LookupEntry>& races,
                                                const std::vector<dnd::LookupEntry>& alignments,
                                                const std::vector<dnd::Weapon>& weapons) {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    nameEdit_ = new QLineEdit(page);
    classCombo_ = new QComboBox(page);
    populateLookupCombo(classCombo_, classes);
    raceCombo_ = new QComboBox(page);
    populateLookupCombo(raceCombo_, races);
    alignmentCombo_ = new QComboBox(page);
    populateLookupCombo(alignmentCombo_, alignments);
    weaponCombo_ = new QComboBox(page);
    for (const auto& weapon : weapons) {
        weaponCombo_->addItem(QString::fromStdString(weapon.name), weapon.weapon_id);
    }
    levelSpin_ = new QSpinBox(page);
    levelSpin_->setRange(1, 20);
    levelSpin_->setValue(1);
    currentHpSpin_ = new QSpinBox(page);
    currentHpSpin_->setRange(0, 999);
    maxHpSpin_ = new QSpinBox(page);
    maxHpSpin_->setRange(0, 999);
    armorClassSpin_ = new QSpinBox(page);
    armorClassSpin_->setRange(0, 30);
    armorClassSpin_->setValue(10);

    form->addRow("Name", nameEdit_);
    form->addRow("Class", classCombo_);
    form->addRow("Race", raceCombo_);
    form->addRow("Alignment", alignmentCombo_);
    form->addRow("Weapon", weaponCombo_);
    form->addRow("Level", levelSpin_);
    form->addRow("Current HP", currentHpSpin_);
    form->addRow("Max HP", maxHpSpin_);
    form->addRow("Armor Class", armorClassSpin_);
    return page;
}

QWidget* CharacterEditorDialog::buildAbilityScoresTab() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        auto ability = static_cast<dnd::Ability>(i);
        auto* spin = new QSpinBox(page);
        spin->setRange(1, 30);
        spin->setValue(10);
        abilityScoreSpins_[i] = spin;
        form->addRow(dnd::ability_name(ability), spin);
    }
    return page;
}

QWidget* CharacterEditorDialog::buildProficienciesTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* savesGroup = new QGroupBox("Saving Throws", page);
    auto* savesLayout = new QVBoxLayout(savesGroup);
    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        auto ability = static_cast<dnd::Ability>(i);
        auto* box = new QCheckBox(dnd::ability_name(ability), savesGroup);
        saveProficiencyBoxes_[i] = box;
        savesLayout->addWidget(box);
    }
    layout->addWidget(savesGroup);

    auto* skillsGroup = new QGroupBox("Skills", page);
    auto* skillsLayout = new QVBoxLayout(skillsGroup);
    for (int a = 0; a < dnd::kAbilityCount; ++a) {
        auto ability = static_cast<dnd::Ability>(a);
        auto* abilityGroup = new QGroupBox(dnd::ability_name(ability), skillsGroup);
        auto* abilityLayout = new QVBoxLayout(abilityGroup);
        for (int s = 0; s < dnd::kSkillCount; ++s) {
            auto skill = static_cast<dnd::Skill>(s);
            if (dnd::ability_for_skill(skill) != ability) continue;
            auto* box = new QCheckBox(dnd::skill_name(skill), abilityGroup);
            skillProficiencyBoxes_[s] = box;
            abilityLayout->addWidget(box);
        }
        skillsLayout->addWidget(abilityGroup);
    }
    layout->addWidget(skillsGroup);

    return page;
}

void CharacterEditorDialog::populateFrom(const dnd::Character& existing) {
    nameEdit_->setText(QString::fromStdString(existing.character_name));
    classCombo_->setCurrentIndex(classCombo_->findData(existing.class_id));
    raceCombo_->setCurrentIndex(raceCombo_->findData(existing.race));
    alignmentCombo_->setCurrentIndex(alignmentCombo_->findData(existing.alignment));
    weaponCombo_->setCurrentIndex(weaponCombo_->findData(existing.weapon_id));
    levelSpin_->setValue(existing.level);
    currentHpSpin_->setValue(existing.current_hp);
    maxHpSpin_->setValue(existing.max_hp);
    armorClassSpin_->setValue(existing.armor_class);

    abilityScoreSpins_[static_cast<int>(dnd::Ability::Strength)]->setValue(existing.strength);
    abilityScoreSpins_[static_cast<int>(dnd::Ability::Dexterity)]->setValue(existing.dexterity);
    abilityScoreSpins_[static_cast<int>(dnd::Ability::Constitution)]->setValue(existing.constitution);
    abilityScoreSpins_[static_cast<int>(dnd::Ability::Intelligence)]->setValue(existing.intelligence);
    abilityScoreSpins_[static_cast<int>(dnd::Ability::Wisdom)]->setValue(existing.wisdom);
    abilityScoreSpins_[static_cast<int>(dnd::Ability::Charisma)]->setValue(existing.charisma);

    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        saveProficiencyBoxes_[i]->setChecked((existing.save_proficiencies >> i) & 1u);
    }
    for (int i = 0; i < dnd::kSkillCount; ++i) {
        skillProficiencyBoxes_[i]->setChecked((existing.skill_proficiencies >> i) & 1u);
    }
}

dnd::Character CharacterEditorDialog::character() const {
    dnd::Character character;
    character.character_name = nameEdit_->text().trimmed().toStdString();
    character.class_id = classCombo_->currentData().toInt();
    character.race = raceCombo_->currentData().toInt();
    character.alignment = alignmentCombo_->currentData().toInt();
    character.weapon_id = weaponCombo_->currentData().toInt();
    character.level = levelSpin_->value();
    character.current_hp = currentHpSpin_->value();
    character.max_hp = maxHpSpin_->value();
    character.armor_class = armorClassSpin_->value();

    character.strength = abilityScoreSpins_[static_cast<int>(dnd::Ability::Strength)]->value();
    character.dexterity = abilityScoreSpins_[static_cast<int>(dnd::Ability::Dexterity)]->value();
    character.constitution = abilityScoreSpins_[static_cast<int>(dnd::Ability::Constitution)]->value();
    character.intelligence = abilityScoreSpins_[static_cast<int>(dnd::Ability::Intelligence)]->value();
    character.wisdom = abilityScoreSpins_[static_cast<int>(dnd::Ability::Wisdom)]->value();
    character.charisma = abilityScoreSpins_[static_cast<int>(dnd::Ability::Charisma)]->value();

    std::uint32_t saveMask = 0;
    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        if (saveProficiencyBoxes_[i]->isChecked()) saveMask |= (1u << i);
    }
    character.save_proficiencies = saveMask;

    std::uint32_t skillMask = 0;
    for (int i = 0; i < dnd::kSkillCount; ++i) {
        if (skillProficiencyBoxes_[i]->isChecked()) skillMask |= (1u << i);
    }
    character.skill_proficiencies = skillMask;

    return character;
}
