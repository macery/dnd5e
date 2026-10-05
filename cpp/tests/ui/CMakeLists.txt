# Offscreen UI tests: the app's dialogs built against an in-memory database
# with QT_QPA_PLATFORM=offscreen, clicked through, and checked. They turn
# the walkthroughs used during development into regression tests (README
# TODO: Qt UI tests). Catch2 comes from ../CMakeLists.txt's FetchContent.
FetchContent_GetProperties(Catch2)

add_executable(dnd_ui_tests
    main.cpp
    test_window_sizes.cpp
    test_dialogs.cpp
    test_combat_ui.cpp
    test_adventure_ui.cpp
    test_shop_ui.cpp
    test_magic_items_ui.cpp
    test_campaign_ui.cpp
    test_spellbook_ui.cpp
    test_subclass_ui.cpp
    test_class_options_ui.cpp
    test_scene_ui.cpp
)
target_link_libraries(dnd_ui_tests PRIVATE dnd_ui Catch2::Catch2)

# One CTest entry for the whole program: it needs a QApplication, which
# Catch2's per-test discovery would also have to start at build time.
add_test(NAME ui_tests COMMAND dnd_ui_tests)
set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
