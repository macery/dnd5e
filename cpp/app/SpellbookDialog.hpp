#pragma once

#include <QDialog>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/dice.hpp"
#include "dnd/lookup_entry.hpp"
#include "dnd/repository.hpp"
#include "dnd/spell.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

// A caster's spells outside combat: spellcasting stats and slots, learning
// and forgetting spells within the class allowance (how new spells are
// picked after leveling up), and casting healing or utility spells on
// themselves. Damage spells are cast from Combat Encounter. Every change is
// persisted immediately via Repository.
class SpellbookDialog : public QDialog {
    Q_OBJECT

public:
    SpellbookDialog(dnd::Repository& repository, dnd::Character character,
                    const std::vector<dnd::LookupEntry>& classes, QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();
    void onCastClicked();
    void onForgetClicked();
    void onLearnClicked();
    void onPrepareClicked();

private:
    void reload();
    const dnd::Spell* selectedSpell() const;
    // The spells the character would know after adding `extra`.
    std::vector<dnd::Spell> knownPlus(const dnd::Spell& extra) const;

    dnd::Repository& repository_;
    dnd::Character character_;
    const dnd::ClassInfo* classInfo_ = nullptr;
    std::vector<dnd::Spell> allSpells_;
    std::vector<dnd::Spell> known_;

    dnd::SystemRandomSource randomSource_;
    dnd::Dice dice_{randomSource_};

    QLabel* statsLabel_ = nullptr;
    QLabel* slotsLabel_ = nullptr;
    QLabel* countsLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QComboBox* slotCombo_ = nullptr;
    QPushButton* castButton_ = nullptr;
    QPushButton* forgetButton_ = nullptr;
    QPushButton* prepareButton_ = nullptr;  // Wizards: prepare or unprepare a spellbook spell
    QLabel* prepareLabel_ = nullptr;        // when prepared spells can change
    QComboBox* learnCombo_ = nullptr;
    QPushButton* learnButton_ = nullptr;
    bool isWizard() const;
    bool preparesFromList() const;  // Cleric, Druid, Paladin: the list is what's prepared
    QLabel* messageLabel_ = nullptr;
};
