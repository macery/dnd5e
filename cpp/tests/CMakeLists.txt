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
    test_combat.cpp
    test_dice.cpp
    test_migrations.cpp
    test_repository.cpp
    test_rules.cpp
)
target_link_libraries(dnd_tests PRIVATE dndcore Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(dnd_tests)
