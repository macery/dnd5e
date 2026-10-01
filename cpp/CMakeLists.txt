cmake_minimum_required(VERSION 3.16)
project(dnd_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SQLite3 REQUIRED)

# CMake renamed the imported target from SQLite::SQLite3 to SQLite3::SQLite3
# in a recent release; pick whichever this CMake's FindSQLite3 module provides
# so the build works on both older and newer CMake versions without warnings.
if(TARGET SQLite3::SQLite3)
    set(sqlite3_target SQLite3::SQLite3)
else()
    set(sqlite3_target SQLite::SQLite3)
endif()

add_library(dndcore
    src/adventure.cpp
    src/character_creation.cpp
    src/class_features.cpp
    src/combat.cpp
    src/dice.cpp
    src/effects.cpp
    src/encounter.cpp
    src/equipment.cpp
    src/inventory.cpp
    src/level_up.cpp
    src/migrations.cpp
    src/progression.cpp
    src/repository.cpp
    src/rules.cpp
    src/spellcasting.cpp
    src/turn.cpp
)
target_include_directories(dndcore PUBLIC include)
target_link_libraries(dndcore PUBLIC ${sqlite3_target})

enable_testing()
add_subdirectory(tests)

# dndcore stays UI-agnostic: the app subdirectory is only built when Qt6
# Widgets is actually available, so this library and its tests keep building
# and passing on a machine without Qt installed at all.
find_package(Qt6 QUIET COMPONENTS Widgets)
if(TARGET Qt6::Widgets)
    add_subdirectory(app)
else()
    message(STATUS "Qt6 Widgets not found - skipping native UI target (dndcore and its tests still build)")
endif()
