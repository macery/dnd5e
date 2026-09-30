#include "CharacterCreationWizard.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>
#include <QWizardPage>

#include <algorithm>
#include <array>
#include <stdexcept>

#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

namespace {

using dnd::Ability;
using dnd::Skill;

QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

int idFor(const std::vector<dnd::LookupEntry>& entries, const std::string& name) {
    for (const auto& entry : entries) {
        if (entry.name == name) return entry.id;
    }
    return 0;
}

QString nameFor(const std::vector<dnd::LookupEntry>& entries, int id) {
    for (const auto& entry : entries) {
        if (entry.id == id) return q(entry.name);
    }
    return "-";
}

QString skillList(const std::vector<Skill>& skills) {
    QStringList names;
    for (Skill skill : skills) names << dnd::skill_name(skill);
    return names.isEmpty() ? "none" : names.join(", ");
}

QString bonusList(const dnd::RaceInfo& race) {
    QStringList parts;
    for (int i = 0; i < dnd::kAbilityCount; ++i) {
        int bonus = race.ability_bonuses[static_cast<size_t>(i)];
        if (bonus != 0) parts << QString("%1 %2").arg(formatModifier(bonus), dnd::ability_name(static_cast<Ability>(i)));
    }
    if (race.flexible_bonus_count > 0) parts << QString("+1 to %1 others of your choice").arg(race.flexible_bonus_count);
    return parts.join(", ");
}

enum PageId { ClassPageId, RacePageId, AbilitiesPageId, BackgroundPageId, SkillsPageId, SpellsPageId,
              DetailsPageId, EquipmentPageId, ReviewPageId };

// The character as chosen so far -- level 1, final ability scores -- for
// rules that depend on scores before the whole character can be built.
dnd::Character previewCharacter(const CharacterCreationContext& context) {
    dnd::Character character;
    character.level = 1;
    auto scores = dnd::final_ability_scores(context.choices);
    character.strength = scores[0];
    character.dexterity = scores[1];
    character.constitution = scores[2];
    character.intelligence = scores[3];
    character.wisdom = scores[4];
    character.charisma = scores[5];
    return character;
}

// True if the chosen class picks spells at level 1.
bool picksSpellsAtCreation(const CharacterCreationContext& context) {
    const dnd::ClassInfo* info = dnd::find_class_info(context.choices.class_name);
    if (!info) return false;
    auto allowance = dnd::spell_allowance(previewCharacter(context), *info);
    return allowance.cantrips + allowance.spells > 0;
}

QString spellKindText(const dnd::Spell& spell) {
    return q(dnd::spell_summary(spell));
}

// Shared by every page: an inline error label and the common context.
class CreationPage : public QWizardPage {
public:
    CreationPage(CharacterCreationContext& context, const QString& title, const QString& subtitle)
        : context_(context) {
        setTitle(title);
        setSubTitle(subtitle);
        errorLabel_ = new QLabel(this);
        errorLabel_->setStyleSheet("color: #b00020; font-weight: bold;");
        errorLabel_->setWordWrap(true);
    }

protected:
    // Shows `error` and returns false if there is one; clears it otherwise.
    bool accepted(const std::optional<std::string>& error) {
        errorLabel_->setText(error ? q(*error) : QString());
        return !error;
    }

    CharacterCreationContext& context_;
    QLabel* errorLabel_ = nullptr;
};

// A list on the left, details for the selection on the right. Used for the
// class, race, and background pages.
class ListChoicePage : public CreationPage {
public:
    ListChoicePage(CharacterCreationContext& context, const QString& title, const QString& subtitle)
        : CreationPage(context, title, subtitle) {
        list_ = new QListWidget(this);
        details_ = new QLabel(this);
        details_->setWordWrap(true);
        details_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        details_->setTextFormat(Qt::RichText);

        row_ = new QHBoxLayout();
        row_->addWidget(list_, 1);
        row_->addWidget(details_, 2);

        layout_ = new QVBoxLayout(this);
        layout_->addLayout(row_, 1);
        layout_->addWidget(errorLabel_);

        connect(list_, &QListWidget::currentRowChanged, this, [this](int) {
            details_->setText(list_->currentItem() ? detailsFor(list_->currentItem()->text().toStdString()) : "");
            onSelectionChanged();
            emit completeChanged();
        });
    }

    bool isComplete() const override { return list_->currentItem() != nullptr; }

protected:
    virtual QString detailsFor(const std::string& name) const = 0;
    virtual void onSelectionChanged() {}

    std::string selectedName() const {
        return list_->currentItem() ? list_->currentItem()->text().toStdString() : std::string();
    }

    void selectName(const std::string& name) {
        auto matches = list_->findItems(q(name), Qt::MatchExactly);
        if (!matches.isEmpty()) list_->setCurrentItem(matches.front());
    }

    QListWidget* list_ = nullptr;
    QLabel* details_ = nullptr;
    QHBoxLayout* row_ = nullptr;
    QVBoxLayout* layout_ = nullptr;
};

// ---------------------------------------------------------------- Class

class ClassPage : public ListChoicePage {
public:
    explicit ClassPage(CharacterCreationContext& context)
        : ListChoicePage(context, "Choose a Class",
                         "Your class is your adventuring profession: it sets your hit points, which saving throws "
                         "you're proficient in, and which skills you can train.") {
        for (const auto& info : dnd::class_infos()) {
            if (idFor(context_.classes, info.name) != 0) list_->addItem(q(info.name));
        }
    }

    bool validatePage() override {
        std::string name = selectedName();
        context_.choices.class_name = name;
        context_.choices.class_id = idFor(context_.classes, name);
        return accepted(name.empty() ? std::optional<std::string>("Choose a class.") : std::nullopt);
    }

protected:
    QString detailsFor(const std::string& name) const override {
        const dnd::ClassInfo* info = dnd::find_class_info(name);
        if (!info) return "";
        return QString("<h3>%1</h3><p>%2</p>"
                       "<p><b>Hit die:</b> d%3 (level-1 HP = %3 + Constitution modifier)<br>"
                       "<b>Saving throws:</b> %4, %5<br>"
                       "<b>Skills:</b> choose %6 from %7<br>"
                       "<b>Suggested weapon:</b> %8</p>")
            .arg(q(info->name), q(info->summary))
            .arg(info->hit_die)
            .arg(dnd::ability_name(info->saving_throws[0]), dnd::ability_name(info->saving_throws[1]))
            .arg(info->skill_choice_count)
            .arg(skillList(info->skill_options), q(info->suggested_weapon));
    }
};

// ---------------------------------------------------------------- Race

class RacePage : public ListChoicePage {
public:
    explicit RacePage(CharacterCreationContext& context)
        : ListChoicePage(context, "Choose a Race",
                         "Your race adds bonuses to your ability scores and gives you a few innate traits.") {
        for (const auto& info : dnd::race_infos()) {
            if (idFor(context_.races, info.name) != 0) list_->addItem(q(info.name));
        }

        flexibleGroup_ = new QGroupBox("Your +1 bonuses", this);
        auto* flexibleLayout = new QHBoxLayout(flexibleGroup_);
        for (auto*& combo : flexibleCombos_) {
            combo = new QComboBox(flexibleGroup_);
            flexibleLayout->addWidget(combo);
        }
        layout_->insertWidget(1, flexibleGroup_);
        flexibleGroup_->hide();
    }

    bool validatePage() override {
        std::string name = selectedName();
        context_.choices.race_name = name;
        context_.choices.race_id = idFor(context_.races, name);
        context_.choices.flexible_bonus_abilities.clear();

        const dnd::RaceInfo* race = dnd::find_race_info(name);
        for (int i = 0; race && i < race->flexible_bonus_count; ++i) {
            context_.choices.flexible_bonus_abilities.push_back(
                static_cast<Ability>(flexibleCombos_[static_cast<size_t>(i)]->currentData().toInt()));
        }
        return accepted(dnd::validate_flexible_bonuses(context_.choices));
    }

protected:
    QString detailsFor(const std::string& name) const override {
        const dnd::RaceInfo* race = dnd::find_race_info(name);
        if (!race) return "";
        QStringList traits;
        for (const auto& trait : race->traits) traits << q(trait);
        QString skills = race->skill_proficiencies.empty() ? "" :
            QString("<b>Skill proficiency:</b> %1<br>").arg(skillList(race->skill_proficiencies));
        return QString("<h3>%1</h3><p><b>Ability bonuses:</b> %2<br><b>Speed:</b> %3 ft.<br>%4"
                       "<b>Traits:</b> %5</p>")
            .arg(q(race->name), bonusList(*race))
            .arg(race->speed)
            .arg(skills, traits.join(", "));
    }

    void onSelectionChanged() override {
        const dnd::RaceInfo* race = dnd::find_race_info(selectedName());
        int count = race ? race->flexible_bonus_count : 0;
        flexibleGroup_->setVisible(count > 0);
        for (size_t i = 0; i < flexibleCombos_.size(); ++i) {
            QComboBox* combo = flexibleCombos_[i];
            combo->clear();
            combo->setVisible(static_cast<int>(i) < count);
            if (!race) continue;
            for (int a = 0; a < dnd::kAbilityCount; ++a) {
                if (race->ability_bonuses[static_cast<size_t>(a)] == 0) {
                    combo->addItem(dnd::ability_name(static_cast<Ability>(a)), a);
                }
            }
            if (combo->count() > static_cast<int>(i)) combo->setCurrentIndex(static_cast<int>(i));
        }
    }

private:
    QGroupBox* flexibleGroup_ = nullptr;
    std::array<QComboBox*, 2> flexibleCombos_{};
};

// ---------------------------------------------------------- Ability scores

class AbilityScoresPage : public CreationPage {
public:
    explicit AbilityScoresPage(CharacterCreationContext& context)
        : CreationPage(context, "Ability Scores",
                       "Generate six scores, then decide which ability gets which. Racial bonuses are added on "
                       "top.") {
        auto* methodRow = new QHBoxLayout();
        methodGroup_ = new QButtonGroup(this);
        const std::array<std::pair<const char*, const char*>, 3> methods = {{
            {"Standard Array", "Use the fixed values 15, 14, 13, 12, 10, 8. Balanced and quick."},
            {"Roll 4d6", "Roll four d6 six times, dropping the lowest die each time. Exciting, but random."},
            {"Point Buy", "Spend 27 points; every score starts at 8 and can go up to 15."},
        }};
        for (int i = 0; i < static_cast<int>(methods.size()); ++i) {
            auto* radio = new QRadioButton(methods[static_cast<size_t>(i)].first, this);
            radio->setToolTip(methods[static_cast<size_t>(i)].second);
            methodGroup_->addButton(radio, i);
            methodRow->addWidget(radio);
        }
        methodGroup_->button(0)->setChecked(true);
        rollButton_ = new QPushButton("Roll!", this);
        methodRow->addStretch(1);
        methodRow->addWidget(rollButton_);

        poolLabel_ = new QLabel(this);

        auto* grid = new QGridLayout();
        const QStringList headers = {"Ability", "Score", "Racial", "Final", "Modifier"};
        for (int c = 0; c < headers.size(); ++c) {
            auto* header = new QLabel(QString("<b>%1</b>").arg(headers[c]), this);
            grid->addWidget(header, 0, c);
        }
        for (int a = 0; a < dnd::kAbilityCount; ++a) {
            auto ability = static_cast<Ability>(a);
            auto* name = new QLabel(dnd::ability_name(ability), this);
            name->setToolTip(dnd::ability_description(ability));
            assignCombos_[a] = new QComboBox(this);
            pointSpins_[a] = new QSpinBox(this);
            pointSpins_[a]->setRange(dnd::kPointBuyMin, dnd::kPointBuyMax);
            racialLabels_[a] = new QLabel(this);
            finalLabels_[a] = new QLabel(this);
            modifierLabels_[a] = new QLabel(this);

            auto* scoreCell = new QHBoxLayout();
            scoreCell->addWidget(assignCombos_[a]);
            scoreCell->addWidget(pointSpins_[a]);
            grid->addWidget(name, a + 1, 0);
            grid->addLayout(scoreCell, a + 1, 1);
            grid->addWidget(racialLabels_[a], a + 1, 2);
            grid->addWidget(finalLabels_[a], a + 1, 3);
            grid->addWidget(modifierLabels_[a], a + 1, 4);

            connect(assignCombos_[a], QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this](int) { refreshTotals(); });
            connect(pointSpins_[a], QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this](int) { refreshTotals(); });
        }

        hintLabel_ = new QLabel(this);
        hintLabel_->setWordWrap(true);

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(methodRow);
        layout->addWidget(poolLabel_);
        layout->addLayout(grid);
        layout->addWidget(hintLabel_);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);

        connect(methodGroup_, &QButtonGroup::idClicked, this, [this](int) { applyMethod(); });
        connect(rollButton_, &QPushButton::clicked, this, [this]() {
            rolled_ = dnd::roll_ability_scores(context_.dice);
            hasRolled_ = true;
            applyMethod();
        });
        applyMethod();
    }

    void initializePage() override {
        const dnd::ClassInfo* info = dnd::find_class_info(context_.choices.class_name);
        hintLabel_->setText(info ? QString("Tip: %1s make saving throws with %2 and %3, so high scores there help.")
                                       .arg(q(info->name), dnd::ability_name(info->saving_throws[0]),
                                            dnd::ability_name(info->saving_throws[1]))
                                 : QString());
        refreshTotals();
    }

    bool validatePage() override {
        collect();
        return accepted(dnd::validate_ability_scores(context_.choices));
    }

private:
    dnd::AbilityScoreMethod method() const {
        return static_cast<dnd::AbilityScoreMethod>(methodGroup_->checkedId());
    }

    std::array<int, dnd::kAbilityCount> pool() const {
        return method() == dnd::AbilityScoreMethod::Rolled ? rolled_ : dnd::kStandardArray;
    }

    // Rebuilds the controls for the selected method.
    void applyMethod() {
        bool pointBuy = method() == dnd::AbilityScoreMethod::PointBuy;
        bool rolledMethod = method() == dnd::AbilityScoreMethod::Rolled;
        if (rolledMethod && !hasRolled_) {
            rolled_ = dnd::roll_ability_scores(context_.dice);
            hasRolled_ = true;
        }
        rollButton_->setVisible(rolledMethod);

        auto values = pool();
        std::sort(values.begin(), values.end(), std::greater<>());
        for (int a = 0; a < dnd::kAbilityCount; ++a) {
            assignCombos_[a]->setVisible(!pointBuy);
            pointSpins_[a]->setVisible(pointBuy);

            assignCombos_[a]->blockSignals(true);
            assignCombos_[a]->clear();
            for (int value : values) assignCombos_[a]->addItem(QString::number(value), value);
            assignCombos_[a]->setCurrentIndex(a);  // one of each by default
            assignCombos_[a]->blockSignals(false);
        }

        QStringList poolText;
        for (int value : values) poolText << QString::number(value);
        poolLabel_->setText(pointBuy ? QString() : "Values to assign: " + poolText.join(", "));
        poolLabel_->setVisible(!pointBuy);
        refreshTotals();
    }

    void collect() {
        auto& choices = context_.choices;
        choices.method = method();
        choices.score_pool = pool();
        for (int a = 0; a < dnd::kAbilityCount; ++a) {
            choices.base_scores[static_cast<size_t>(a)] = choices.method == dnd::AbilityScoreMethod::PointBuy
                ? pointSpins_[a]->value()
                : assignCombos_[a]->currentData().toInt();
        }
    }

    void refreshTotals() {
        collect();
        auto final = dnd::final_ability_scores(context_.choices);
        for (int a = 0; a < dnd::kAbilityCount; ++a) {
            auto index = static_cast<size_t>(a);
            int racial = final[index] - context_.choices.base_scores[index];
            racialLabels_[a]->setText(racial ? formatModifier(racial) : "");
            finalLabels_[a]->setText(QString("<b>%1</b>").arg(final[index]));
            modifierLabels_[a]->setText(formatModifier(dnd::ability_modifier(final[index])));
        }

        if (method() == dnd::AbilityScoreMethod::PointBuy) {
            int spent = dnd::point_buy_total(context_.choices.base_scores);
            poolLabel_->setVisible(true);
            poolLabel_->setText(QString("Points spent: %1 / %2").arg(spent).arg(dnd::kPointBuyBudget));
        }
        errorLabel_->clear();
    }

    QButtonGroup* methodGroup_ = nullptr;
    QPushButton* rollButton_ = nullptr;
    QLabel* poolLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    std::array<QComboBox*, dnd::kAbilityCount> assignCombos_{};
    std::array<QSpinBox*, dnd::kAbilityCount> pointSpins_{};
    std::array<QLabel*, dnd::kAbilityCount> racialLabels_{};
    std::array<QLabel*, dnd::kAbilityCount> finalLabels_{};
    std::array<QLabel*, dnd::kAbilityCount> modifierLabels_{};
    std::array<int, dnd::kAbilityCount> rolled_{};
    bool hasRolled_ = false;
};

// ------------------------------------------------------------ Background

class BackgroundPage : public ListChoicePage {
public:
    explicit BackgroundPage(CharacterCreationContext& context)
        : ListChoicePage(context, "Choose a Background",
                         "Your background is what you did before adventuring. It gives you two skill "
                         "proficiencies.") {
        for (const auto& info : dnd::background_infos()) list_->addItem(q(info.name));
    }

    bool validatePage() override {
        context_.choices.background_name = selectedName();
        return accepted(context_.choices.background_name.empty() ? std::optional<std::string>("Choose a background.")
                                                                 : std::nullopt);
    }

protected:
    QString detailsFor(const std::string& name) const override {
        const dnd::BackgroundInfo* info = dnd::find_background_info(name);
        if (!info) return "";
        return QString("<h3>%1</h3><p>%2</p><p><b>Skill proficiencies:</b> %3, %4</p>")
            .arg(q(info->name), q(info->summary), dnd::skill_name(info->skill_proficiencies[0]),
                 dnd::skill_name(info->skill_proficiencies[1]));
    }
};

// ---------------------------------------------------------------- Skills

class SkillsPage : public CreationPage {
public:
    explicit SkillsPage(CharacterCreationContext& context)
        : CreationPage(context, "Choose Skills",
                       "Skills you're proficient in add your proficiency bonus to checks. Skills from your race "
                       "and background are already checked.") {
        statusLabel_ = new QLabel(this);
        statusLabel_->setWordWrap(true);

        auto* grid = new QGridLayout();
        for (int s = 0; s < dnd::kSkillCount; ++s) {
            auto skill = static_cast<Skill>(s);
            boxes_[s] = new QCheckBox(this);
            boxes_[s]->setToolTip(dnd::skill_description(skill));
            grid->addWidget(boxes_[s], s % 9, s / 9);
            connect(boxes_[s], &QCheckBox::toggled, this, [this](bool) { refreshStatus(); });
        }

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(statusLabel_);
        layout->addLayout(grid);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);
    }

    void initializePage() override {
        const auto& choices = context_.choices;
        const dnd::ClassInfo* classInfo = dnd::find_class_info(choices.class_name);
        const dnd::RaceInfo* race = dnd::find_race_info(choices.race_name);
        const dnd::BackgroundInfo* background = dnd::find_background_info(choices.background_name);
        if (!classInfo || !race || !background) return;
        plan_ = dnd::skill_plan(*classInfo, *race, *background);

        for (int s = 0; s < dnd::kSkillCount; ++s) {
            auto skill = static_cast<Skill>(s);
            bool granted = contains(plan_.granted, skill);
            bool classOption = contains(plan_.class_options, skill);
            bool previouslyChosen = contains(choices.chosen_skills, skill);

            QString text = QString("%1 (%2)").arg(dnd::skill_name(skill),
                                                   dnd::ability_name(dnd::ability_for_skill(skill)));
            if (granted) text += "  — from race/background";
            boxes_[s]->blockSignals(true);
            boxes_[s]->setText(text);
            QFont font = boxes_[s]->font();
            font.setBold(classOption);
            boxes_[s]->setFont(font);
            boxes_[s]->setChecked(granted || (previouslyChosen && (classOption || plan_.free_choice_count > 0)));
            boxes_[s]->setEnabled(!granted && (classOption || plan_.free_choice_count > 0));
            boxes_[s]->blockSignals(false);
        }
        refreshStatus();
    }

    bool validatePage() override {
        context_.choices.chosen_skills = chosen();
        return accepted(dnd::validate_skills(context_.choices));
    }

    int nextId() const override { return picksSpellsAtCreation(context_) ? SpellsPageId : DetailsPageId; }

private:
    static bool contains(const std::vector<Skill>& skills, Skill skill) {
        return std::find(skills.begin(), skills.end(), skill) != skills.end();
    }

    std::vector<Skill> chosen() const {
        std::vector<Skill> skills;
        for (int s = 0; s < dnd::kSkillCount; ++s) {
            auto skill = static_cast<Skill>(s);
            if (boxes_[s]->isChecked() && !contains(plan_.granted, skill)) skills.push_back(skill);
        }
        return skills;
    }

    void refreshStatus() {
        int total = plan_.class_choice_count + plan_.free_choice_count;
        QString text = QString("Choose <b>%1</b> from your class list (in <b>bold</b>)").arg(plan_.class_choice_count);
        if (plan_.free_choice_count > 0) text += QString(" plus <b>%1</b> of any skill").arg(plan_.free_choice_count);
        text += QString(". Chosen: <b>%1 / %2</b>").arg(chosen().size()).arg(total);
        statusLabel_->setText(text);
        errorLabel_->clear();
    }

    QLabel* statusLabel_ = nullptr;
    std::array<QCheckBox*, dnd::kSkillCount> boxes_{};
    dnd::SkillPlan plan_;
};

// ---------------------------------------------------------------- Spells

class SpellsPage : public CreationPage {
public:
    explicit SpellsPage(CharacterCreationContext& context)
        : CreationPage(context, "Choose Spells",
                       "Cantrips can be cast at will. Spells of 1st level and up each use a spell slot, which "
                       "comes back after a long rest.") {
        statsLabel_ = new QLabel(this);
        statsLabel_->setWordWrap(true);
        cantripsLabel_ = new QLabel(this);
        spellsLabel_ = new QLabel(this);
        cantripList_ = new QListWidget(this);
        spellList_ = new QListWidget(this);

        auto* columns = new QHBoxLayout();
        auto* left = new QVBoxLayout();
        left->addWidget(cantripsLabel_);
        left->addWidget(cantripList_, 1);
        auto* right = new QVBoxLayout();
        right->addWidget(spellsLabel_);
        right->addWidget(spellList_, 1);
        columns->addLayout(left, 1);
        columns->addLayout(right, 1);

        auto* note = new QLabel("<i>Hover a spell for its description. Utility spells have no automated effect "
                                "yet -- casting one just uses the slot.</i>", this);
        note->setWordWrap(true);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(statsLabel_);
        layout->addLayout(columns, 1);
        layout->addWidget(note);
        layout->addWidget(errorLabel_);

        for (auto* list : {cantripList_, spellList_}) {
            connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { refreshCounts(); });
        }
    }

    void initializePage() override {
        const dnd::ClassInfo* info = dnd::find_class_info(context_.choices.class_name);
        if (!info) return;
        character_ = previewCharacter(context_);
        allowance_ = dnd::spell_allowance(character_, *info);

        statsLabel_->setText(
            QString("<b>%1</b> casts with <b>%2</b>: spell attack %3, spell save DC %4. %5")
                .arg(q(info->name), dnd::ability_name(info->spellcasting.ability))
                .arg(formatModifier(dnd::spell_attack_bonus(character_, *info)))
                .arg(dnd::spell_save_dc(character_, *info))
                .arg(info->spellcasting.preparation == dnd::SpellPreparation::Known
                         ? "You know these spells permanently."
                         : "These are your prepared spells (changing them after a long rest comes later)."));

        // Rebuild the lists for this class, keeping earlier picks that still apply.
        const auto& previous = context_.chosen_spell_ids;
        for (auto* list : {cantripList_, spellList_}) {
            list->blockSignals(true);
            list->clear();
        }
        for (const auto& spell : context_.spells) {
            const auto& classes = spell.class_names;
            if (std::find(classes.begin(), classes.end(), info->name) == classes.end()) continue;
            if (spell.level > allowance_.max_spell_level) continue;
            auto* list = spell.level == 0 ? cantripList_ : spellList_;
            auto* item = new QListWidgetItem(QString("%1 — %2").arg(q(spell.name), spellKindText(spell)), list);
            item->setData(Qt::UserRole, spell.spell_id);
            item->setToolTip(QString("<b>%1</b> (%2 %3)<br>%4<br><i>Range: %5 · Duration: %6</i>")
                                 .arg(q(spell.name), spell.level == 0 ? "cantrip," : "1st-level", q(spell.school),
                                      q(spell.description), q(spell.range_text), q(spell.duration_text)));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            bool wasChosen = std::find(previous.begin(), previous.end(), spell.spell_id) != previous.end();
            item->setCheckState(wasChosen ? Qt::Checked : Qt::Unchecked);
        }
        for (auto* list : {cantripList_, spellList_}) list->blockSignals(false);
        refreshCounts();
    }

    bool validatePage() override {
        std::vector<dnd::Spell> chosen;
        context_.chosen_spell_ids.clear();
        for (auto* list : {cantripList_, spellList_}) {
            for (int row = 0; row < list->count(); ++row) {
                if (list->item(row)->checkState() != Qt::Checked) continue;
                int id = list->item(row)->data(Qt::UserRole).toInt();
                context_.chosen_spell_ids.push_back(id);
                for (const auto& spell : context_.spells) {
                    if (spell.spell_id == id) chosen.push_back(spell);
                }
            }
        }
        const dnd::ClassInfo* info = dnd::find_class_info(context_.choices.class_name);
        if (!info) return accepted(std::string("Choose a class."));
        return accepted(dnd::validate_spell_selection(character_, *info, chosen, /*require_full=*/true));
    }

private:
    static int checkedCount(const QListWidget* list) {
        int count = 0;
        for (int row = 0; row < list->count(); ++row) count += list->item(row)->checkState() == Qt::Checked;
        return count;
    }

    void refreshCounts() {
        cantripsLabel_->setText(
            QString("<b>Cantrips</b>: %1 / %2").arg(checkedCount(cantripList_)).arg(allowance_.cantrips));
        spellsLabel_->setText(
            QString("<b>1st-level spells</b>: %1 / %2").arg(checkedCount(spellList_)).arg(allowance_.spells));
        errorLabel_->clear();
    }

    QLabel* statsLabel_ = nullptr;
    QLabel* cantripsLabel_ = nullptr;
    QLabel* spellsLabel_ = nullptr;
    QListWidget* cantripList_ = nullptr;
    QListWidget* spellList_ = nullptr;
    dnd::Character character_;
    dnd::SpellAllowance allowance_;
};

// ---------------------------------------------------------------- Details

class DetailsPage : public CreationPage {
public:
    explicit DetailsPage(CharacterCreationContext& context)
        : CreationPage(context, "Name and Alignment", "Who is your character?") {
        nameEdit_ = new QLineEdit(this);
        alignmentCombo_ = new QComboBox(this);
        alignmentCombo_->setToolTip(
            "Your character's moral and ethical outlook, from Lawful Good to Chaotic Evil. It's a roleplaying "
            "guide, not a rule.");
        for (const auto& entry : context_.alignments) alignmentCombo_->addItem(q(entry.name), entry.id);
        int neutralGood = alignmentCombo_->findText("Neutral Good");
        if (neutralGood >= 0) alignmentCombo_->setCurrentIndex(neutralGood);

        auto* form = new QFormLayout();
        form->addRow("Name", nameEdit_);
        form->addRow("Alignment", alignmentCombo_);

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);
    }

    bool validatePage() override {
        context_.choices.name = nameEdit_->text().trimmed().toStdString();
        context_.choices.alignment_id = alignmentCombo_->currentData().toInt();
        if (auto error = dnd::validate_details(context_.choices)) return accepted(error);

        const auto& names = context_.existing_names;
        if (std::find(names.begin(), names.end(), context_.choices.name) != names.end()) {
            return accepted("There's already a character named " + context_.choices.name + ".");
        }
        return accepted(std::nullopt);
    }

private:
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* alignmentCombo_ = nullptr;
};

// ------------------------------------------------------------- Equipment

class EquipmentPage : public CreationPage {
public:
    explicit EquipmentPage(CharacterCreationContext& context)
        : CreationPage(context, "Starting Equipment", "Pick a weapon and some gear to start with.") {
        weaponCombo_ = new QComboBox(this);
        for (const auto& weapon : context_.weapons) weaponCombo_->addItem(q(weapon.name), weapon.weapon_id);
        weaponDetails_ = new QLabel(this);

        QStringList pack;
        for (const auto& item : dnd::explorers_pack()) pack << QString("%1 × %2").arg(item.quantity).arg(q(item.item_name));
        packBox_ = new QCheckBox("Take the Explorer's Pack", this);
        packBox_->setChecked(true);
        auto* packContents = new QLabel(pack.join(", "), this);
        packContents->setWordWrap(true);

        auto* form = new QFormLayout();
        form->addRow("Weapon", weaponCombo_);
        form->addRow("", weaponDetails_);
        form->addRow(packBox_);
        form->addRow(packContents);

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);

        connect(weaponCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { refreshWeaponDetails(); });
    }

    void initializePage() override {
        // Re-suggest only when the class changes, so a manual pick survives Back/Next.
        if (context_.choices.class_name != suggestedFor_) {
            suggestedFor_ = context_.choices.class_name;
            if (const dnd::ClassInfo* info = dnd::find_class_info(suggestedFor_)) {
                int index = weaponCombo_->findText(q(info->suggested_weapon));
                if (index >= 0) weaponCombo_->setCurrentIndex(index);
            }
        }
        refreshWeaponDetails();
    }

    bool validatePage() override {
        context_.choices.weapon_id = weaponCombo_->currentData().toInt();
        context_.take_explorers_pack = packBox_->isChecked();
        return accepted(std::nullopt);
    }

private:
    void refreshWeaponDetails() {
        for (const auto& weapon : context_.weapons) {
            if (weapon.weapon_id != weaponCombo_->currentData().toInt()) continue;
            QStringList properties;
            if (weapon.is_finesse) properties << "finesse (uses STR or DEX)";
            if (weapon.is_ranged) properties << "ranged (uses DEX)";
            weaponDetails_->setText(QString("%1d%2 damage%3")
                                        .arg(weapon.damage_dice_count)
                                        .arg(weapon.damage_dice_sides)
                                        .arg(properties.isEmpty() ? "" : ", " + properties.join(", ")));
        }
    }

    QComboBox* weaponCombo_ = nullptr;
    QLabel* weaponDetails_ = nullptr;
    QCheckBox* packBox_ = nullptr;
    std::string suggestedFor_;
};

// ---------------------------------------------------------------- Review

class ReviewPage : public CreationPage {
public:
    explicit ReviewPage(CharacterCreationContext& context)
        : CreationPage(context, "Review", "Here's your new character. Click Finish to save, or go Back to change "
                                          "anything.") {
        summary_ = new QLabel(this);
        summary_->setTextFormat(Qt::RichText);
        summary_->setWordWrap(true);
        summary_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(summary_, 1);
        layout->addWidget(errorLabel_);
    }

    void initializePage() override {
        try {
            dnd::Character character = dnd::build_character(context_.choices);
            summary_->setText(render(character));
            accepted(std::nullopt);
        } catch (const std::invalid_argument& error) {
            summary_->clear();
            accepted(std::string(error.what()));
        }
    }

    bool validatePage() override { return accepted(dnd::validate_choices(context_.choices)); }

private:
    QString render(const dnd::Character& character) const {
        const auto& choices = context_.choices;
        const dnd::RaceInfo* race = dnd::find_race_info(choices.race_name);

        QString abilities;
        for (int a = 0; a < dnd::kAbilityCount; ++a) {
            auto ability = static_cast<Ability>(a);
            int score = dnd::ability_score(character, ability);
            QString save = dnd::has_save_proficiency(character, ability) ? " (save ✓)" : "";
            abilities += QString("<tr><td>%1</td><td align='right'><b>%2</b></td><td>%3%4</td></tr>")
                             .arg(dnd::ability_name(ability))
                             .arg(score)
                             .arg(formatModifier(dnd::ability_modifier(score)), save);
        }

        QStringList skills;
        for (int s = 0; s < dnd::kSkillCount; ++s) {
            auto skill = static_cast<Skill>(s);
            if (dnd::has_skill_proficiency(character, skill)) {
                skills << QString("%1 %2").arg(dnd::skill_name(skill), formatModifier(dnd::skill_bonus(character, skill)));
            }
        }

        QString weaponText = "-";
        for (const auto& weapon : context_.weapons) {
            if (weapon.weapon_id != character.weapon_id) continue;
            int damageModifier = dnd::weapon_attack_ability_modifier(character, weapon);
            weaponText = QString("%1: %2 to hit, %3d%4%5")
                             .arg(q(weapon.name), formatModifier(dnd::weapon_attack_bonus(character, weapon)))
                             .arg(weapon.damage_dice_count)
                             .arg(weapon.damage_dice_sides)
                             .arg(damageModifier ? formatModifier(damageModifier) : "");
        }

        return QString("<h2>%1</h2>"
                       "<p>Level 1 %2 %3 · %4 · %5</p>"
                       "<p><b>HP</b> %6 · <b>AC</b> %7 · <b>Initiative</b> %8 · <b>Speed</b> %9 ft. · "
                       "<b>Proficiency</b> %10 · <b>Passive Perception</b> %11</p>"
                       "<table cellspacing='4'>%12</table>"
                       "<p><b>Skills:</b> %13</p>"
                       "<p><b>Weapon:</b> %14<br><b>Gear:</b> %15</p>%16")
            .arg(q(character.character_name), q(choices.race_name), q(choices.class_name),
                 q(choices.background_name), nameFor(context_.alignments, character.alignment))
            .arg(character.max_hp)
            .arg(character.armor_class)
            .arg(formatModifier(dnd::initiative(character)))
            .arg(race ? race->speed : 30)
            .arg(formatModifier(dnd::proficiency_bonus(character.level)))
            .arg(dnd::passive_perception(character))
            .arg(abilities, skills.join(", "), weaponText,
                 context_.take_explorers_pack ? "Explorer's Pack" : "none", spellsText(character));
    }

    QString spellsText(const dnd::Character& character) const {
        const dnd::ClassInfo* info = dnd::find_class_info(context_.choices.class_name);
        if (!info || !picksSpellsAtCreation(context_)) return "";
        QStringList names;
        for (int id : context_.chosen_spell_ids) {
            for (const auto& spell : context_.spells) {
                if (spell.spell_id == id) names << q(spell.name);
            }
        }
        return QString("<p><b>Spellcasting:</b> %1, spell attack %2, save DC %3, %4 × 1st-level slots<br>"
                       "<b>Spells:</b> %5</p>")
            .arg(dnd::ability_name(info->spellcasting.ability),
                 formatModifier(dnd::spell_attack_bonus(character, *info)))
            .arg(dnd::spell_save_dc(character, *info))
            .arg(dnd::spell_slots(*info, 1)[0])
            .arg(names.join(", "));
    }

    QLabel* summary_ = nullptr;
};

}  // namespace

CharacterCreationWizard::CharacterCreationWizard(dnd::Repository& repository, int player_id,
                                                 const std::vector<dnd::Weapon>& weapons, QWidget* parent)
    : QWizard(parent), repository_(repository), context_(std::make_unique<CharacterCreationContext>()) {
    setWindowTitle("Create a Character");

    context_->choices.player_id = player_id;
    context_->classes = repository_.get_classes();
    context_->races = repository_.get_races();
    context_->alignments = repository_.get_alignments();
    context_->weapons = weapons;
    context_->spells = repository_.get_spells();
    for (const auto& character : repository_.get_characters()) context_->existing_names.push_back(character.character_name);

    setPage(ClassPageId, new ClassPage(*context_));
    setPage(RacePageId, new RacePage(*context_));
    setPage(AbilitiesPageId, new AbilityScoresPage(*context_));
    setPage(BackgroundPageId, new BackgroundPage(*context_));
    setPage(SkillsPageId, new SkillsPage(*context_));
    setPage(SpellsPageId, new SpellsPage(*context_));
    setPage(DetailsPageId, new DetailsPage(*context_));
    setPage(EquipmentPageId, new EquipmentPage(*context_));
    setPage(ReviewPageId, new ReviewPage(*context_));

    resize(680, 560);
}

CharacterCreationWizard::~CharacterCreationWizard() = default;

void CharacterCreationWizard::accept() {
    dnd::Character character;
    try {
        character = dnd::build_character(context_->choices);
    } catch (const std::invalid_argument& error) {
        QMessageBox::warning(this, "Create a Character", error.what());
        return;
    }

    int character_id = repository_.add_character(character);
    if (picksSpellsAtCreation(*context_)) {
        for (int spell_id : context_->chosen_spell_ids) repository_.add_character_spell(character_id, spell_id);
    }
    if (context_->take_explorers_pack) {
        auto items = repository_.get_items();
        for (const auto& starting : dnd::explorers_pack()) {
            for (const auto& item : items) {
                if (item.name == starting.item_name) {
                    repository_.add_to_inventory(character_id, item.item_id, starting.quantity);
                    break;
                }
            }
        }
    }
    QWizard::accept();
}
