find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

# Every window and dialog, as a library: the app links it, and so do the
# offscreen UI tests (tests/ui).
add_library(dnd_ui STATIC
    MainWindow.cpp
    CharacterTableModel.cpp
    DiceRollerWidget.cpp
    CharacterCreationWizard.cpp
    CharacterEditorDialog.cpp
    CharacterSheetDialog.cpp
    InitiativeTrackerDialog.cpp
    InventoryDialog.cpp
    ShopDialog.cpp
    SpellbookDialog.cpp
    LevelUpDialog.cpp
    ImportDialog.cpp
    AdventureDialogs.cpp
    WindowSizing.cpp
    MonsterListDialog.cpp
    CombatEncounterDialog.cpp
    AdventurePickerDialog.cpp
)
target_include_directories(dnd_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dnd_ui PUBLIC dndcore Qt6::Widgets)

add_executable(dnd_app main.cpp)
target_link_libraries(dnd_app PRIVATE dnd_ui)
