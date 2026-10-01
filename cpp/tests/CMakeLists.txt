include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

add_executable(dnd_tests
    test_adventure.cpp
    test_adventure_run.cpp
    test_character_creation.cpp
    test_character_io.cpp
    test_combat.cpp
    test_currency.cpp
    test_damage.cpp
    test_dice.cpp
    test_effects.cpp
    test_encounter.cpp
    test_encounter_difficulty.cpp
    test_equipment.cpp
    test_inventory.cpp
    test_level_up.cpp
    test_migrations.cpp
    test_progression.cpp
    test_repository.cpp
    test_rules.cpp
    test_spellcasting.cpp
    test_turn.cpp
)
target_link_libraries(dnd_tests PRIVATE dndcore Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(dnd_tests)
