#include "core/AppSettings.h"
#include "core/ImageLoader.h"
#include "core/LibraryDatabase.h"
#include "ui/MainWindow.h"
#include "ui/QuickViewerWindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LuminaPhoto"));
    app.setOrganizationName(QStringLiteral("Lumina"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/lumina.png")));

    auto& settings = lumina::AppSettings::instance();
    lumina::AppSettings::applyApplicationPalette(app, settings.effectiveDarkTheme());

    lumina::LibraryDatabase db;
    if (!db.initialize()) {
        QMessageBox::critical(
            nullptr,
            lumina::trUi(QStringLiteral("Ошибка базы данных"), QStringLiteral("Database Error")),
            lumina::trUi(QStringLiteral("Не удалось инициализировать локальный индекс SQLite."),
                         QStringLiteral("Failed to initialize the local SQLite database.")));
        return 1;
    }

    std::unique_ptr<lumina::MainWindow> mainWin;
    std::unique_ptr<lumina::QuickViewerWindow> quickWin;

    // Fast Picasa Photo Viewer path when launched via Windows Explorer double-click on an image / .psd
    QString launchFile;
    const QStringList args = QCoreApplication::arguments();
    if (args.size() >= 2) {
        const QString candidate = args.at(1);
        if (QFileInfo::exists(candidate) && lumina::ImageLoader::isSupportedFile(candidate)) {
            launchFile = candidate;
        }
    }

    if (!launchFile.isEmpty()) {
        quickWin = std::make_unique<lumina::QuickViewerWindow>(&db, launchFile);
        QObject::connect(quickWin.get(), &lumina::QuickViewerWindow::openInLibraryRequested,
                         [&](const QString& filePath) {
                             if (!mainWin) {
                                 mainWin = std::make_unique<lumina::MainWindow>(&db);
                             }
                             mainWin->openPhotoInEditor(filePath);
                             mainWin->show();
                             mainWin->raise();
                             mainWin->activateWindow();
                             quickWin->hide();
                         });
        quickWin->show();
    } else {
        mainWin = std::make_unique<lumina::MainWindow>(&db);
        mainWin->show();
    }

    return app.exec();
}
