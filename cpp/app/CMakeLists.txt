find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(dnd_app
    main.cpp
    MainWindow.cpp
    CharacterTableModel.cpp
    DiceRollerWidget.cpp
    CharacterCreationWizard.cpp
    CharacterEditorDialog.cpp
    CharacterSheetDialog.cpp
    InitiativeTrackerDialog.cpp
    InventoryDialog.cpp
    SpellbookDialog.cpp
    LevelUpDialog.cpp
    ImportDialog.cpp
    AdventureDialogs.cpp
    WindowSizing.cpp
    MonsterListDialog.cpp
    CombatEncounterDialog.cpp
    AdventurePickerDialog.cpp
)
target_link_libraries(dnd_app PRIVATE dndcore Qt6::Widgets)
