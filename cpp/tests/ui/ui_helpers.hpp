#pragma once

// Shared helpers for the offscreen UI tests: a seeded in-memory database
// with characters, and ways to find and click widgets by their text.

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <string>
#include <vector>

#include "dnd/repository.hpp"

namespace ui_test {

inline void settle() {
    QApplication::processEvents();
}

// The first visible button whose text starts with `prefix`, or nullptr.
inline QPushButton* button(QWidget& root, const QString& prefix) {
    for (auto* b : root.findChildren<QPushButton*>()) {
        if (b->isVisible() && b->text().startsWith(prefix)) return b;
    }
    return nullptr;
}

// The first combo box holding an item that starts with `prefix`, or nullptr.
inline QComboBox* comboWith(QWidget& root, const QString& prefix) {
    for (auto* combo : root.findChildren<QComboBox*>()) {
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemText(i).startsWith(prefix)) return combo;
        }
    }
    return nullptr;
}

// Selects the item starting with `prefix` in whichever combo has it.
inline bool choose(QWidget& root, const QString& prefix) {
    QComboBox* combo = comboWith(root, prefix);
    if (!combo) return false;
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemText(i).startsWith(prefix)) {
            combo->setCurrentIndex(i);
            settle();
            return true;
        }
    }
    return false;
}

// All visible label text, joined: for "does the window say X" checks.
inline QString visibleText(QWidget& root) {
    QStringList parts;
    for (auto* label : root.findChildren<QLabel*>()) {
        if (label->isVisible() && !label->text().isEmpty()) parts << label->text();
    }
    return parts.join("\n");
}

// An in-memory database with one player ("Adam") and helpers to add
// characters with gear.
struct World {
    dnd::Repository repo{":memory:"};
    int player = repo.add_player("Adam");

    int classId(const std::string& name) {
        for (const auto& c : repo.get_classes()) {
            if (c.name == name) return c.id;
        }
        return 0;
    }

    int itemId(const std::string& name) {
        for (const auto& i : repo.get_items()) {
            if (i.name == name) return i.item_id;
        }
        return 0;
    }

    // A character of `class_name` at `level`, with STR 16 / DEX 14 / CON 14 /
    // WIS 14 / CHA 14 and full HP, wearing and wielding `gear`.
    dnd::Character add(const std::string& name, const std::string& class_name, int level,
                       std::vector<std::string> gear = {}) {
        dnd::Character c;
        c.character_name = name;
        c.player_id = player;
        c.class_id = classId(class_name);
        c.race = 1;
        c.alignment = 1;
        c.level = level;
        c.strength = 16;
        c.dexterity = 14;
        c.constitution = 14;
        c.intelligence = 12;
        c.wisdom = 14;
        c.charisma = 14;
        c.max_hp = 10 * level;
        c.current_hp = 10 * level;
        c.character_id = repo.add_character(c);
        for (const auto& item : gear) {
            repo.add_to_inventory(c.character_id, itemId(item));
            repo.set_equipped(c.character_id, itemId(item), true);
        }
        return c;
    }

    dnd::Character get(int character_id) {
        for (const auto& c : repo.get_characters()) {
            if (c.character_id == character_id) return c;
        }
        return {};
    }
};

}  // namespace ui_test
