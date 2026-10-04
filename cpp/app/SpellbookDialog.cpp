#include "SpellbookDialog.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "WindowSizing.hpp"
#include "dnd/spellbook.hpp"
#include "dnd/spellcasting.hpp"
#include "dnd/subclass.hpp"

namespace {

QString q(const std::string& text) {
    return QString::fromStdString(text);
}

QString formatModifier(int value) {
    return value >= 0 ? QString("+%1").arg(value) : QString::number(value);
}

QString ordinal(int level) {
    return QString::fromStdString(dnd::ordinal(level));
}

QString effectText(const dnd::Spell& spell) {
    return q(dnd::spell_summary(spell));
}

QString tooltipFor(const dnd::Spell& spell) {
    return QString("<b>%1</b> (%2, %3)<br>%4<br><i>Range: %5 · Duration: %6</i>")
        .arg(q(spell.name), spell.level == 0 ? "cantrip" : ordinal(spell.level) + " level", q(spell.school),
             q(spell.description), q(spell.range_text), q(spell.duration_text));
}

}  // namespace

SpellbookDialog::SpellbookDialog(dnd::Repository& repository, dnd::Character character,
                                 const std::vector<dnd::LookupEntry>& classes, QWidget* parent)
    : QDialog(parent), repository_(repository), character_(std::move(character)) {
    setWindowTitle(QString("Spellbook — %1").arg(q(character_.character_name)));
    classInfo_ = dnd::find_class_info_by_id(classes, character_.class_id);
    allSpells_ = repository_.get_spells();

    statsLabel_ = new QLabel(this);
    statsLabel_->setWordWrap(true);
    slotsLabel_ = new QLabel(this);
    slotsLabel_->setToolTip("Spell slots remaining / total. A long rest restores them; Warlocks also regain "
                            "theirs on a short rest.");
    countsLabel_ = new QLabel(this);

    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels({"Spell", "Level", "Effect", "Casting Time", "Prepared"});
    table_->horizontalHeaderItem(4)->setToolTip(
        "A Wizard prepares INT modifier + level spells from the spellbook each day; only those can be cast. "
        "Cantrips are always ready.");
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->setVisible(false);

    auto* castRow = new QHBoxLayout();
    slotCombo_ = new QComboBox(this);
    slotCombo_->setToolTip("Which spell slot to use. A higher slot makes many spells stronger.");
    castButton_ = new QPushButton("Cast on Self", this);
    forgetButton_ = new QPushButton("Forget", this);
    prepareButton_ = new QPushButton("Prepare", this);
    castRow->addStretch(1);
    castRow->addWidget(slotCombo_);
    castRow->addWidget(castButton_);
    castRow->addWidget(prepareButton_);
    castRow->addWidget(forgetButton_);
    prepareLabel_ = new QLabel(this);
    prepareLabel_->setWordWrap(true);

    auto* learnRow = new QHBoxLayout();
    learnCombo_ = new QComboBox(this);
    learnButton_ = new QPushButton("Learn", this);
    learnRow->addWidget(new QLabel("Learn:", this));
    learnRow->addWidget(learnCombo_, 1);
    learnRow->addWidget(learnButton_);

    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(statsLabel_);
    layout->addWidget(slotsLabel_);
    layout->addWidget(countsLabel_);
    layout->addWidget(prepareLabel_);
    layout->addWidget(table_, 1);
    layout->addLayout(castRow);
    layout->addWidget(messageLabel_);
    layout->addLayout(learnRow);
    layout->addWidget(buttons);
    fitToScreen(this, QSize(720, 520), "Spellbook");

    connect(table_, &QTableWidget::itemSelectionChanged, this, &SpellbookDialog::onSelectionChanged);
    connect(castButton_, &QPushButton::clicked, this, &SpellbookDialog::onCastClicked);
    connect(forgetButton_, &QPushButton::clicked, this, &SpellbookDialog::onForgetClicked);
    connect(learnButton_, &QPushButton::clicked, this, &SpellbookDialog::onLearnClicked);
    connect(prepareButton_, &QPushButton::clicked, this, &SpellbookDialog::onPrepareClicked);

    if (!classInfo_ || !dnd::is_spellcaster(*classInfo_)) {
        statsLabel_->setText(QString("%1's class doesn't cast spells.").arg(q(character_.character_name)));
        for (QWidget* widget : std::initializer_list<QWidget*>{slotsLabel_, countsLabel_, table_, slotCombo_,
                                                               castButton_, forgetButton_, learnCombo_,
                                                               learnButton_, prepareButton_}) {
            widget->setEnabled(false);
        }
        return;
    }
    reload();
}

void SpellbookDialog::reload() {
    // The character's spells, plus subclass spells that are always prepared (shown, never counted).
    known_ = dnd::spells_ready(repository_, character_);
    subclassSpells_ = dnd::always_prepared_spell_names(character_);
    const auto& info = *classInfo_;
    auto allowance = dnd::spell_allowance(character_, info);

    statsLabel_->setText(QString("<b>%1</b> · casts with %2 · spell attack <b>%3</b> · spell save DC <b>%4</b>")
                             .arg(q(info.name), dnd::ability_name(info.spellcasting.ability),
                                  formatModifier(dnd::spell_attack_bonus(character_, info)))
                             .arg(dnd::spell_save_dc(character_, info)));

    QStringList slotParts;  // not `slots`: that's a Qt macro
    auto totals = dnd::spell_slots(info, character_.level);
    for (int level = 1; level <= dnd::kMaxSpellLevel; ++level) {
        int total = totals[static_cast<size_t>(level - 1)];
        if (total == 0) continue;
        slotParts << QString("%1: %2/%3")
                         .arg(ordinal(level))
                         .arg(dnd::slots_remaining(character_, info, level))
                         .arg(total);
    }
    slotsLabel_->setText("Spell slots — " + (slotParts.isEmpty() ? QString("none yet") : slotParts.join(" · ")));

    int cantrips = 0;
    int spells = 0;
    for (const auto& spell : known_) {
        if (!isSubclassSpell(spell)) (spell.level == 0 ? cantrips : spells) += 1;
    }
    QString spellsWord = info.spellcasting.preparation == dnd::SpellPreparation::Known ? "Spells known"
                         : isWizard()                                                  ? "Spellbook"
                                                                                       : "Spells prepared";
    QString counts = QString("Cantrips: %1 / %2 · %3: %4 / %5")
                         .arg(cantrips)
                         .arg(allowance.cantrips)
                         .arg(spellsWord)
                         .arg(spells)
                         .arg(allowance.spells);
    if (isWizard()) {
        counts += QString(" · Prepared: %1 / %2")
                      .arg(dnd::prepared_leveled_count(known_))
                      .arg(dnd::prepared_spell_count(character_, info));
    }
    countsLabel_->setText(counts);

    // When prepared spells can change: after a long rest, until the next fight.
    std::string refusal = dnd::preparation_refusal(character_, info);
    prepareLabel_->setVisible(dnd::prepares_spells(info));
    prepareLabel_->setText(refusal.empty()
                               ? "You can change prepared spells now (after a long rest, until your next fight)."
                               : q(refusal));
    table_->setColumnHidden(4, !isWizard());
    forgetButton_->setText(preparesFromList() ? "Unprepare" : isWizard() ? "Remove from Book" : "Forget");
    learnButton_->setText(preparesFromList() ? "Prepare" : isWizard() ? "Add to Spellbook" : "Learn");

    int selectedRow = table_->currentRow();
    table_->setRowCount(static_cast<int>(known_.size()));
    for (int row = 0; row < static_cast<int>(known_.size()); ++row) {
        const auto& spell = known_[static_cast<size_t>(row)];
        auto* name = new QTableWidgetItem(q(spell.name));
        name->setToolTip(tooltipFor(spell));
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, new QTableWidgetItem(spell.level == 0 ? "Cantrip" : ordinal(spell.level)));
        table_->setItem(row, 2, new QTableWidgetItem(effectText(spell)));
        QString time = spell.casting_time == dnd::CastingTime::Action        ? "Action"
                       : spell.casting_time == dnd::CastingTime::BonusAction ? "Bonus Action"
                                                                              : "Reaction";
        table_->setItem(row, 3, new QTableWidgetItem(time));
        table_->setItem(row, 4, new QTableWidgetItem(spell.level == 0 ? "always" : spell.prepared ? "✓" : ""));
        if (isSubclassSpell(spell)) {
            name->setText(q(spell.name) + QString(" (%1)").arg(q(character_.subclass)));
            name->setToolTip(tooltipFor(spell) + "<br><i>Always prepared by your subclass; it doesn't count against "
                                                 "your spells.</i>");
        }
    }
    if (selectedRow >= 0 && selectedRow < table_->rowCount()) table_->selectRow(selectedRow);

    learnCombo_->clear();
    for (const auto& spell : allSpells_) {
        bool alreadyKnown = std::any_of(known_.begin(), known_.end(),
                                        [&](const dnd::Spell& k) { return k.spell_id == spell.spell_id; });
        if (alreadyKnown) continue;
        if (!dnd::on_spell_list(spell, info.name, character_)) continue;  // a patron's expanded list too
        if (spell.level > allowance.max_spell_level) continue;
        learnCombo_->addItem(QString("%1 (%2) — %3")
                                 .arg(q(spell.name), spell.level == 0 ? "cantrip" : ordinal(spell.level),
                                      effectText(spell)),
                             spell.spell_id);
        learnCombo_->setItemData(learnCombo_->count() - 1, tooltipFor(spell), Qt::ToolTipRole);
    }
    // A Cleric's, Druid's, or Paladin's list is what's prepared: it changes only in the window.
    bool listLocked = preparesFromList() && !refusal.empty();
    learnButton_->setEnabled(learnCombo_->count() > 0 && !listLocked);
    learnButton_->setToolTip(listLocked ? q(refusal) : QString());
    onSelectionChanged();
}

const dnd::Spell* SpellbookDialog::selectedSpell() const {
    int row = table_->currentRow();
    if (table_->selectedItems().isEmpty() || row < 0 || row >= static_cast<int>(known_.size())) return nullptr;
    return &known_[static_cast<size_t>(row)];
}

std::vector<dnd::Spell> SpellbookDialog::knownPlus(const dnd::Spell& extra) const {
    std::vector<dnd::Spell> spells;
    for (const auto& spell : known_) {
        if (!isSubclassSpell(spell)) spells.push_back(spell);  // subclass spells don't count
    }
    spells.push_back(extra);
    return spells;
}

void SpellbookDialog::onSelectionChanged() {
    const dnd::Spell* spell = selectedSpell();
    std::string refusal = dnd::preparation_refusal(character_, *classInfo_);
    bool listLocked = preparesFromList() && !refusal.empty();
    forgetButton_->setEnabled(spell != nullptr && !listLocked && !isSubclassSpell(*spell));
    forgetButton_->setToolTip(listLocked ? q(refusal) : QString());
    prepareButton_->setVisible(isWizard());
    prepareButton_->setText(spell && spell->prepared && spell->level > 0 ? "Unprepare" : "Prepare");
    prepareButton_->setEnabled(spell && spell->level > 0 && refusal.empty());
    prepareButton_->setToolTip(!refusal.empty() ? q(refusal) : QString());

    slotCombo_->clear();
    bool castable = spell && spell->prepared && spell->casting_time != dnd::CastingTime::Reaction &&
                    (spell->resolution == dnd::SpellResolution::Heal ||
                     spell->resolution == dnd::SpellResolution::Utility);
    if (spell && spell->level > 0) {
        for (int level = spell->level; level <= dnd::kMaxSpellLevel; ++level) {
            int remaining = dnd::slots_remaining(character_, *classInfo_, level);
            if (remaining == 0) continue;
            slotCombo_->addItem(QString("%1-level slot (%2 left)").arg(ordinal(level)).arg(remaining), level);
        }
        castable = castable && slotCombo_->count() > 0;
    }
    slotCombo_->setVisible(spell && spell->level > 0);
    castButton_->setEnabled(castable);
    castButton_->setToolTip(spell && !castable && spell->resolution != dnd::SpellResolution::Heal &&
                                    spell->resolution != dnd::SpellResolution::Utility
                                ? "Cast this spell from Combat Encounter: damage, buffs, and stabilizing only matter "
                                  "in a fight."
                                : "");
}

void SpellbookDialog::onCastClicked() {
    const dnd::Spell* selected = selectedSpell();
    if (!selected) return;
    dnd::Spell spell = *selected;  // reload() below invalidates `selected`
    int slotLevel = spell.level == 0 ? 0 : slotCombo_->currentData().toInt();
    if (character_.dead) {
        messageLabel_->setText(QString("%1 is dead and can't cast spells.").arg(q(character_.character_name)));
        return;
    }

    if (spell.resolution == dnd::SpellResolution::Heal && character_.current_hp >= character_.max_hp) {
        messageLabel_->setText(QString("%1 is already at full HP.").arg(q(character_.character_name)));
        return;
    }
    if (slotLevel > 0 && !dnd::spend_spell_slot(character_, *classInfo_, slotLevel)) {
        messageLabel_->setText("No spell slots of that level left.");
        return;
    }

    auto result = dnd::cast_spell(dice_, spell, dnd::caster_stats(character_, *classInfo_), slotLevel, {{}});
    QString slotText = slotLevel > 0 ? QString(" with a %1-level slot").arg(ordinal(slotLevel)) : "";
    if (spell.resolution == dnd::SpellResolution::Heal) {
        int before = character_.current_hp;
        int amount = result.effects.front().amount;
        if (character_.subclass == "Life Domain" && spell.level > 0) amount += 2 + slotLevel;  // Disciple of Life
        character_.current_hp = std::min(character_.max_hp, character_.current_hp + amount);
        messageLabel_->setText(QString("Cast %1%2: regained %3 HP (now %4/%5).")
                                   .arg(q(spell.name), slotText)
                                   .arg(character_.current_hp - before)
                                   .arg(character_.current_hp)
                                   .arg(character_.max_hp));
    } else {
        messageLabel_->setText(QString("Cast %1%2. %3").arg(q(spell.name), slotText, q(spell.description)));
    }
    repository_.update_character(character_);
    reload();
}

bool SpellbookDialog::isSubclassSpell(const dnd::Spell& spell) const {
    return std::find(subclassSpells_.begin(), subclassSpells_.end(), spell.name) != subclassSpells_.end();
}

bool SpellbookDialog::isWizard() const {
    return classInfo_ && classInfo_->spellcasting.preparation == dnd::SpellPreparation::Spellbook;
}

bool SpellbookDialog::preparesFromList() const {
    return classInfo_ && classInfo_->spellcasting.preparation == dnd::SpellPreparation::Prepared;
}

void SpellbookDialog::onPrepareClicked() {
    const dnd::Spell* spell = selectedSpell();
    if (!spell) return;
    auto result = dnd::set_prepared(repository_, character_, *classInfo_, spell->spell_id, !spell->prepared);
    messageLabel_->setText(q(result.message));
    reload();
}

void SpellbookDialog::onForgetClicked() {
    const dnd::Spell* spell = selectedSpell();
    if (!spell) return;
    QString name = q(spell->name);
    repository_.remove_character_spell(character_.character_id, spell->spell_id);
    messageLabel_->setText(QString("Forgot %1.").arg(name));
    reload();
}

void SpellbookDialog::onLearnClicked() {
    int spellId = learnCombo_->currentData().toInt();
    auto match = std::find_if(allSpells_.begin(), allSpells_.end(),
                              [spellId](const dnd::Spell& spell) { return spell.spell_id == spellId; });
    if (match == allSpells_.end()) return;

    if (auto error = dnd::validate_spell_selection(character_, *classInfo_, knownPlus(*match), false)) {
        messageLabel_->setText(q(*error) + " Forget one first to swap it out.");
        return;
    }
    dnd::add_known_spell(repository_, character_, *classInfo_, spellId);
    messageLabel_->setText(QString("Learned %1.").arg(q(match->name)));
    reload();
}
