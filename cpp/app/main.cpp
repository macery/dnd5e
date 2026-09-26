#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include <memory>

#include "MainWindow.hpp"
#include "dnd/repository.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("DnD Engine");
    QApplication::setOrganizationName("dnd5e");

    // No hardcoded database path (the old App.config pointed at C:\Repos\dnd5e\...).
    // QStandardPaths resolves the correct per-user data directory on whatever OS
    // this happens to be running on.
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString dbPath = dataDir + "/dnd.db";

    std::unique_ptr<dnd::Repository> repository;
    try {
        repository = std::make_unique<dnd::Repository>(dbPath.toStdString());
    } catch (const std::exception& error) {
        // There's no schema migration system yet (deliberately, while this app
        // has no real users): an older database from before a schema change
        // fails here rather than silently misbehaving. Fail with a clear,
        // actionable message instead of letting the exception escape and
        // std::terminate the whole process.
        QMessageBox::critical(nullptr, "DnD Engine - database error",
            QString("Couldn't open the database:\n\n%1\n\n%2\n\n"
                    "If this database predates a schema change, delete the file above "
                    "and relaunch to let it rebuild.")
                .arg(dbPath)
                .arg(error.what()));
        return 1;
    }

    MainWindow window(*repository);
    window.show();

    return QApplication::exec();
}
