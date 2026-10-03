#include "LevelUpDialog.hpp"

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWizardPage>

#include <algorithm>
#include <stdexcept>

#include "WindowSizing.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/dice.hpp"
#include "dnd/level_up.hpp"
#include "dnd/magic_items.hpp"
#include "dnd/progression.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellbook.hpp"
#include "dnd/spellcasting.hpp"

using dnd::Ability;

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}

enum PageId { HitPointsPageId, AbilityPageId, FightingStylePageId, SpellsPageId, SummaryPageId };

}  // namespace

// Shared by every page: the character, the plan, and the choices so far.
struct LevelUpContext {
    dnd::Character before;
    dnd::Character after;  // set on Finish
    const dnd::ClassInfo* classInfo = nullptr;
    dnd::LevelUpPlan plan;
    dnd::LevelUpChoices choices;
    std::vector<dnd::Spell> allSpells;
    std::vector<dnd::Spell> known;
    std::vector<int> chosenSpellIds;
    dnd::SystemRandomSource randomSource;
    dnd::Dice dice{randomSource};

    // The character with the choices so far applied (the level, HP, and ASI;
    // the Fighting Style once chosen).
    dnd::Character preview() const {
        dnd::Character c = before;
        auto choices = this->choices;
        if (plan.fighting_style && choices.fighting_style.empty() && !plan.fighting_style_options.empty()) {
            choices.fighting_style = dnd::fighting_style_info(plan.fighting_style_options.front())->key;
        }
        try {
            dnd::apply_level_up(c, plan, choices);
        } catch (const std::invalid_argument&) {
            c.level = plan.new_level;  // choices not made yet: just the level
        }
        return c;
    }

    // New cantrips and spells the next level allows, after the ASI (prepared
    // counts follow the casting ability).
    std::pair<int, int> newSpellPicks() const {
        if (!plan.spellcaster || !classInfo) return {0, 0};
        auto allowance = dnd::spell_allowance(preview(), *classInfo);
        int cantrips = 0;
        int spells = 0;
        for (const auto& spell : known) (spell.level == 0 ? cantrips : spells) += 1;
        return {std::max(0, allowance.cantrips - cantrips), std::max(0, allowance.spells - spells)};
    }

    bool pageApplies(int id) const {
        switch (id) {
            case AbilityPageId: return plan.ability_score_improvement;
            case FightingStylePageId: return plan.fighting_style;
            case SpellsPageId: {
                auto [cantrips, spells] = newSpellPicks();
                return cantrips + spells > 0;
            }
            default: return true;
        }
    }

    int nextAfter(int id) const {
        for (int next = id + 1; next <= SummaryPageId; ++next) {
            if (pageApplies(next)) return next;
        }
        return -1;
    }
};

namespace {

class LevelUpPage : public QWizardPage {
public:
    LevelUpPage(LevelUpContext& context, int id, const QString& title, const QString& subtitle)
        : context_(context), id_(id) {
        setTitle(title);
        setSubTitle(subtitle);
        errorLabel_ = new QLabel(this);
        errorLabel_->setStyleSheet("color: #b00020; font-weight: bold;");
        errorLabel_->setWordWrap(true);
    }

    int nextId() const override { return context_.nextAfter(id_); }

protected:
    bool accepted(const std::optional<std::string>& error) {
        errorLabel_->setText(error ? q(*error) : QString());
        return !error;
    }

    LevelUpContext& context_;
    int id_;
    QLabel* errorLabel_ = nullptr;
};

// ------------------------------------------------------------- Hit points

class HitPointsPage : public LevelUpPage {
public:
    explicit HitPointsPage(LevelUpContext& context)
        : LevelUpPage(context, HitPointsPageId, "Hit Points",
                      "Each level adds hit points: take the average of your hit die, or roll it. Either way your "
                      "Constitution modifier is added (at least 1 HP per level).") {
        const auto& plan = context_.plan;
        averageRadio_ = new QRadioButton(QString("Take the average: %1").arg(plan.average_hp), this);
        rollRadio_ = new QRadioButton(QString("Roll 1d%1").arg(plan.hit_die), this);
        auto* group = new QButtonGroup(this);
        group->addButton(averageRadio_);
        group->addButton(rollRadio_);
        averageRadio_->setChecked(true);
        rollButton_ = new QPushButton(QString("Roll 1d%1").arg(plan.hit_die), this);
        rollButton_->setEnabled(false);
        resultLabel_ = new QLabel(this);
        resultLabel_->setWordWrap(true);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(averageRadio_);
        layout->addWidget(rollRadio_);
        layout->addWidget(rollButton_, 0, Qt::AlignLeft);
        layout->addWidget(resultLabel_);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);

        connect(averageRadio_, &QRadioButton::toggled, this, [this](bool) { refresh(); });
        connect(rollButton_, &QPushButton::clicked, this, [this] {
            context_.choices.hp_roll = context_.dice.roll(context_.plan.hit_die, 1).front();
            rollButton_->setEnabled(false);  // one roll per level
            averageRadio_->setEnabled(false);
            refresh();
        });
        refresh();
    }

    bool isComplete() const override {
        return averageRadio_->isChecked() || context_.choices.hp_roll > 0;
    }

private:
    void refresh() {
        auto& choices = context_.choices;
        bool roll = rollRadio_->isChecked();
        choices.hp_method = roll ? dnd::HitPointMethod::Roll : dnd::HitPointMethod::Average;
        rollButton_->setEnabled(roll && choices.hp_roll == 0);

        int con = dnd::ability_modifier(context_.before.constitution);
        if (roll && choices.hp_roll == 0) {
            resultLabel_->setText("Click Roll. You keep whatever comes up.");
        } else {
            int die = roll ? choices.hp_roll : context_.plan.average_hp;
            int gain = std::max(1, die + con);
            resultLabel_->setText(QString("%1 %2 CON %3 = <b>%4 HP</b>. Max HP %5 → %6.")
                                      .arg(roll ? QString("Rolled %1").arg(die) : QString("Average %1").arg(die))
                                      .arg(con >= 0 ? "+" : "−")
                                      .arg(std::abs(con))
                                      .arg(gain)
                                      .arg(context_.before.max_hp)
                                      .arg(context_.before.max_hp + gain) +
                                  (context_.plan.ability_score_improvement
                                       ? "<br>Raising Constitution next adds more (1 HP per level)."
                                       : QString()));
        }
        emit completeChanged();
    }

    QRadioButton* averageRadio_ = nullptr;
    QRadioButton* rollRadio_ = nullptr;
    QPushButton* rollButton_ = nullptr;
    QLabel* resultLabel_ = nullptr;
};

// -------------------------------------------------- Ability Score Improvement

class AbilityPage : public LevelUpPage {
public:
    explicit AbilityPage(LevelUpContext& context)
        : LevelUpPage(context, AbilityPageId, "Ability Score Improvement",
                      "Increase one ability score by 2, or two ability scores by 1 each. No score can go above "
                      "20.") {
        firstCombo_ = new QComboBox(this);
        secondCombo_ = new QComboBox(this);
        for (QComboBox* combo : {firstCombo_, secondCombo_}) {
            for (int a = 0; a < dnd::kAbilityCount; ++a) {
                auto ability = static_cast<Ability>(a);
                combo->addItem(QString("%1 (%2)").arg(dnd::ability_name(ability))
                                   .arg(dnd::ability_score(context_.before, ability)),
                               a);
            }
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refresh(); });
        }
        previewLabel_ = new QLabel(this);
        previewLabel_->setWordWrap(true);

        auto* form = new QFormLayout();
        form->addRow("+1 to", firstCombo_);
        form->addRow("+1 to", secondCombo_);
        auto* hint = new QLabel("Pick the same ability twice for +2.", this);

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(hint);
        layout->addWidget(previewLabel_);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);
        refresh();
    }

    // The choice is recorded only when leaving the page, so levels without
    // an ASI never carry one.
    bool validatePage() override {
        auto picks = selected();
        for (Ability ability : picks) {
            auto count = std::count(picks.begin(), picks.end(), ability);
            if (dnd::ability_score(context_.before, ability) + count > 20) {
                return accepted(std::string(dnd::ability_name(ability)) + " can't go above 20.");
            }
        }
        context_.choices.ability_increases = picks;
        return accepted(std::nullopt);
    }

private:
    std::vector<Ability> selected() const {
        return {static_cast<Ability>(firstCombo_->currentData().toInt()),
                static_cast<Ability>(secondCombo_->currentData().toInt())};
    }

    void refresh() {
        auto picks = selected();
        QStringList lines;
        auto increases = picks;
        std::sort(increases.begin(), increases.end());
        increases.erase(std::unique(increases.begin(), increases.end()), increases.end());
        for (Ability ability : increases) {
            int score = dnd::ability_score(context_.before, ability);
            auto count = std::count(picks.begin(), picks.end(), ability);
            int raised = score + static_cast<int>(count);
            lines << QString("%1 %2 → <b>%3</b> (modifier %4 → %5)")
                         .arg(dnd::ability_name(ability))
                         .arg(score)
                         .arg(raised)
                         .arg(formatModifier(dnd::ability_modifier(score)),
                              formatModifier(dnd::ability_modifier(raised)));
        }
        previewLabel_->setText(lines.join("<br>"));
        errorLabel_->clear();
    }

    QComboBox* firstCombo_ = nullptr;
    QComboBox* secondCombo_ = nullptr;
    QLabel* previewLabel_ = nullptr;
};

// ------------------------------------------------------------ Fighting Style

class FightingStylePage : public LevelUpPage {
public:
    explicit FightingStylePage(LevelUpContext& context)
        : LevelUpPage(context, FightingStylePageId, "Fighting Style",
                      "Adopt a style of fighting as your specialty.") {
        combo_ = new QComboBox(this);
        for (dnd::FightingStyle style : context_.plan.fighting_style_options) {
            const dnd::FightingStyleInfo* info = dnd::fighting_style_info(style);
            combo_->addItem(QString::fromUtf8(info->name), QString(info->key));
            combo_->setItemData(combo_->count() - 1, QString::fromUtf8(info->description), Qt::ToolTipRole);
        }
        description_ = new QLabel(this);
        description_->setWordWrap(true);
        connect(combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int index) { description_->setText(combo_->itemData(index, Qt::ToolTipRole).toString()); });
        description_->setText(combo_->itemData(0, Qt::ToolTipRole).toString());

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(combo_);
        layout->addWidget(description_);
        layout->addStretch(1);
        layout->addWidget(errorLabel_);
    }

    bool validatePage() override {
        context_.choices.fighting_style = combo_->currentData().toString().toStdString();
        return accepted(std::nullopt);
    }

private:
    QComboBox* combo_ = nullptr;
    QLabel* description_ = nullptr;
};

// ---------------------------------------------------------------- Spells

class SpellsPage : public LevelUpPage {
public:
    explicit SpellsPage(LevelUpContext& context)
        : LevelUpPage(context, SpellsPageId, "New Spells",
                      "Your new level lets you learn more spells. Pick them now, or later in the Spellbook.") {
        cantripsLabel_ = new QLabel(this);
        spellsLabel_ = new QLabel(this);
        cantripList_ = new QListWidget(this);
        spellList_ = new QListWidget(this);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(cantripsLabel_);
        layout->addWidget(cantripList_, 1);
        layout->addWidget(spellsLabel_);
        layout->addWidget(spellList_, 2);
        layout->addWidget(errorLabel_);

        for (QListWidget* list : {cantripList_, spellList_}) {
            connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { refreshCounts(); });
        }
    }

    void initializePage() override {
        auto [cantrips, spells] = context_.newSpellPicks();
        cantripLimit_ = cantrips;
        spellLimit_ = spells;
        auto allowance = dnd::spell_allowance(context_.preview(), *context_.classInfo);

        for (QListWidget* list : {cantripList_, spellList_}) {
            list->blockSignals(true);
            list->clear();
        }
        for (const auto& spell : context_.allSpells) {
            const auto& classes = spell.class_names;
            if (std::find(classes.begin(), classes.end(), context_.classInfo->name) == classes.end()) continue;
            if (spell.level > allowance.max_spell_level) continue;
            bool known = std::any_of(context_.known.begin(), context_.known.end(),
                                     [&](const dnd::Spell& k) { return k.spell_id == spell.spell_id; });
            if (known) continue;
            QListWidget* list = spell.level == 0 ? cantripList_ : spellList_;
            if ((spell.level == 0 ? cantripLimit_ : spellLimit_) == 0) continue;
            QString level = spell.level == 0 ? "cantrip" : q(dnd::ordinal(spell.level));
            auto* item = new QListWidgetItem(
                QString("%1 (%2) — %3").arg(q(spell.name), level, q(dnd::spell_summary(spell))), list);
            item->setData(Qt::UserRole, spell.spell_id);
            item->setToolTip(q(spell.description));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            bool chosen = std::find(context_.chosenSpellIds.begin(), context_.chosenSpellIds.end(),
                                    spell.spell_id) != context_.chosenSpellIds.end();
            item->setCheckState(chosen ? Qt::Checked : Qt::Unchecked);
        }
        for (QListWidget* list : {cantripList_, spellList_}) list->blockSignals(false);
        cantripsLabel_->setVisible(cantripLimit_ > 0);
        cantripList_->setVisible(cantripLimit_ > 0);
        spellsLabel_->setVisible(spellLimit_ > 0);
        spellList_->setVisible(spellLimit_ > 0);
        refreshCounts();
    }

    bool validatePage() override {
        if (checked(cantripList_).size() > static_cast<size_t>(cantripLimit_) ||
            checked(spellList_).size() > static_cast<size_t>(spellLimit_)) {
            return accepted("Too many spells chosen.");
        }
        context_.chosenSpellIds = checked(cantripList_);
        auto spells = checked(spellList_);
        context_.chosenSpellIds.insert(context_.chosenSpellIds.end(), spells.begin(), spells.end());

        auto all = context_.known;
        for (int id : context_.chosenSpellIds) {
            for (const auto& spell : context_.allSpells) {
                if (spell.spell_id == id) all.push_back(spell);
            }
        }
        return accepted(dnd::validate_spell_selection(context_.preview(), *context_.classInfo, all, false));
    }

private:
    static std::vector<int> checked(const QListWidget* list) {
        std::vector<int> ids;
        for (int row = 0; row < list->count(); ++row) {
            if (list->item(row)->checkState() == Qt::Checked) ids.push_back(list->item(row)->data(Qt::UserRole).toInt());
        }
        return ids;
    }

    void refreshCounts() {
        bool prepared = context_.classInfo->spellcasting.preparation != dnd::SpellPreparation::Known;
        cantripsLabel_->setText(QString("<b>Cantrips:</b> %1 / %2 new")
                                    .arg(checked(cantripList_).size())
                                    .arg(cantripLimit_));
        spellsLabel_->setText(QString("<b>%1:</b> %2 / %3 new")
                                  .arg(prepared ? "Spells prepared" : "Spells known")
                                  .arg(checked(spellList_).size())
                                  .arg(spellLimit_));
        errorLabel_->clear();
    }

    QLabel* cantripsLabel_ = nullptr;
    QLabel* spellsLabel_ = nullptr;
    QListWidget* cantripList_ = nullptr;
    QListWidget* spellList_ = nullptr;
    int cantripLimit_ = 0;
    int spellLimit_ = 0;
};

// ---------------------------------------------------------------- Summary

class SummaryPage : public LevelUpPage {
public:
    explicit SummaryPage(LevelUpContext& context)
        : LevelUpPage(context, SummaryPageId, "Summary", "Click Finish to level up, or go Back to change anything.") {
        summary_ = new QLabel(this);
        summary_->setTextFormat(Qt::RichText);
        summary_->setWordWrap(true);
        summary_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        summary_->setContentsMargins(4, 4, 4, 4);
        auto* scroll = new QScrollArea(this);  // a long summary scrolls instead of being cut off
        scroll->setWidget(summary_);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(scroll, 1);
        layout->addWidget(errorLabel_);
    }

    void initializePage() override {
        const auto& before = context_.before;
        const auto& plan = context_.plan;
        dnd::Character after = context_.preview();
        QString html = QString("<h3>%1 reaches level %2</h3>").arg(q(before.character_name)).arg(plan.new_level);

        int gain = after.max_hp - before.max_hp;
        html += QString("<p><b>Hit points:</b> +%1 (max HP %2 → %3)</p>").arg(gain).arg(before.max_hp).arg(after.max_hp);

        if (plan.ability_score_improvement) {
            QStringList raised;
            for (int a = 0; a < dnd::kAbilityCount; ++a) {
                auto ability = static_cast<Ability>(a);
                int from = dnd::ability_score(before, ability);
                int to = dnd::ability_score(after, ability);
                if (from != to) raised << QString("%1 %2 → %3").arg(dnd::ability_name(ability)).arg(from).arg(to);
            }
            html += "<p><b>Ability Score Improvement:</b> " + raised.join(", ") + "</p>";
        }
        if (plan.fighting_style) {
            const dnd::FightingStyleInfo* style =
                dnd::fighting_style_info(dnd::fighting_style(after, context_.classInfo->name));
            if (style) {
                html += QString("<p><b>Fighting Style:</b> %1 — %2</p>")
                            .arg(QString::fromUtf8(style->name), QString::fromUtf8(style->description));
            }
        }

        if (!plan.features.empty()) {
            html += "<p><b>New features:</b></p><ul>";
            for (const auto& feature : plan.features) {
                html += QString("<li>%1%2</li>")
                            .arg(QString::fromUtf8(feature.name))
                            .arg(feature.automated ? QString() : QString(" <i>(not automated yet)</i>"));
            }
            html += "</ul>";
        }

        auto changes = dnd::scaling_changes(before, after, *context_.classInfo);
        if (!changes.empty()) {
            html += "<p><b>What changes:</b></p><table cellspacing='4'>";
            for (const auto& change : changes) {
                html += QString("<tr><td>%1</td><td>%2</td><td>→</td><td><b>%3</b></td></tr>")
                            .arg(q(change.label), change.before.empty() ? QString("—") : q(change.before),
                                 q(change.after));
            }
            html += "</table>";
        }

        if (!context_.chosenSpellIds.empty()) {
            QStringList names;
            for (int id : context_.chosenSpellIds) {
                for (const auto& spell : context_.allSpells) {
                    if (spell.spell_id == id) names << q(spell.name);
                }
            }
            html += "<p><b>New spells:</b> " + names.join(", ") + "</p>";
        }

        int more = dnd::pending_levels(before) - 1;
        if (more > 0) {
            html += QString("<p><i>%1 more level%2 to take after this one.</i></p>").arg(more).arg(more == 1 ? "" : "s");
        }
        summary_->setText(html);
    }

private:
    QLabel* summary_ = nullptr;
};

}  // namespace

LevelUpDialog::LevelUpDialog(dnd::Repository& repository, const dnd::Character& character,
                             const std::vector<dnd::LookupEntry>& classes, QWidget* parent)
    : QWizard(parent), repository_(repository), context_(std::make_unique<LevelUpContext>()) {
    context_->before = character;
    context_->after = character;
    context_->classInfo = dnd::find_class_info_by_id(classes, character.class_id);
    if (!context_->classInfo) throw std::invalid_argument("unknown class");
    context_->plan = dnd::level_up_plan(character, *context_->classInfo);
    context_->allSpells = repository_.get_spells();
    context_->known = repository_.get_character_spells(character.character_id);

    setWindowTitle(QString("Level Up — %1 to level %2").arg(q(character.character_name)).arg(context_->plan.new_level));
    setPage(HitPointsPageId, new HitPointsPage(*context_));
    setPage(AbilityPageId, new AbilityPage(*context_));
    setPage(FightingStylePageId, new FightingStylePage(*context_));
    setPage(SpellsPageId, new SpellsPage(*context_));
    setPage(SummaryPageId, new SummaryPage(*context_));
    setButtonText(QWizard::FinishButton, "Level Up");
    fitToScreen(this, QSize(600, 560), "LevelUp");
}

LevelUpDialog::~LevelUpDialog() = default;

const dnd::Character& LevelUpDialog::character() const {
    return context_->after;
}

void LevelUpDialog::accept() {
    dnd::Character leveled = context_->before;
    if (!context_->plan.ability_score_improvement) context_->choices.ability_increases.clear();
    try {
        // An Amulet of Health's HP follows the new level (and any CON increase).
        dnd::without_item_hp(leveled, repository_.get_inventory(leveled.character_id), [&](dnd::Character& c) {
            dnd::apply_level_up(c, context_->plan, context_->choices);
        });
    } catch (const std::invalid_argument& error) {
        QMessageBox::warning(this, "Level Up", error.what());
        return;
    }
    repository_.update_character(leveled);
    for (int spell_id : context_->chosenSpellIds) {
        dnd::add_known_spell(repository_, leveled, *context_->classInfo, spell_id);
    }
    context_->after = leveled;
    QWizard::accept();
}
