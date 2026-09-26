find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(dnd_app
    main.cpp
    MainWindow.cpp
    CharacterTableModel.cpp
    DiceRollerWidget.cpp
    AddCharacterDialog.cpp
    CharacterSheetDialog.cpp
)
target_link_libraries(dnd_app PRIVATE dndcore Qt6::Widgets)
