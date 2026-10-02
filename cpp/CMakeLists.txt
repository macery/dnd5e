cmake_minimum_required(VERSION 3.16)
project(dnd_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sources are UTF-8 (UI text uses "—", "→", "▶", "×"); MSVC otherwise reads
# them in the system code page and garbles those strings on Windows.
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(SQLite3 REQUIRED)

# CMake renamed the imported target from SQLite::SQLite3 to SQLite3::SQLite3
# in a recent release; pick whichever this CMake's FindSQLite3 module provides
# so the build works on both older and newer CMake versions without warnings.
if(TARGET SQLite3::SQLite3)
    set(sqlite3_target SQLite3::SQLite3)
else()
    set(sqlite3_target SQLite::SQLite3)
endif()

# JSON for character export/import (dnd/character_io): header-only, MIT,
# fetched like Catch2. The release tarball is small; the git repo isn't.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(dndcore
    src/adventure.cpp
    src/adventure_run.cpp
    src/character_creation.cpp
    src/character_io.cpp
    src/class_features.cpp
    src/combat.cpp
    src/currency.cpp
    src/shop.cpp
    src/damage.cpp
    src/dice.cpp
    src/effects.cpp
    src/encounter.cpp
    src/encounter_difficulty.cpp
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
target_link_libraries(dndcore PUBLIC ${sqlite3_target} PRIVATE nlohmann_json::nlohmann_json)

enable_testing()
add_subdirectory(tests)

# dndcore stays UI-agnostic: the app subdirectory is only built when Qt6
# Widgets is actually available, so this library and its tests keep building
# and passing on a machine without Qt installed at all.
find_package(Qt6 QUIET COMPONENTS Widgets)
if(TARGET Qt6::Widgets)
    add_subdirectory(app)
    add_subdirectory(tests/ui)  # offscreen UI tests (needs the app's dnd_ui library)
else()
    message(STATUS "Qt6 Widgets not found - skipping native UI target (dndcore and its tests still build)")
endif()
