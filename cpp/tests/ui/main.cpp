// The UI tests' entry point: a QApplication on the offscreen platform, with
// settings (remembered window sizes) in a throwaway folder so tests never
// read or write a real user's settings, then Catch2.
#include <catch2/catch_session.hpp>

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

int main(int argc, char** argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName("dnd5e-ui-tests");
    QApplication::setApplicationName("dnd_ui_tests");

    QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    return Catch::Session().run(argc, argv);
}
