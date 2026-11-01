#include <QApplication>
#include <QStandardPaths>
#include <QDir>

#include <exception>
#include <iostream>

#include "app/qt/main_window.hpp"
#include "infra/logging.hpp"

#include <QMainWindow>

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);
        app.setOrganizationName(QStringLiteral("CameraEmulator"));
        app.setApplicationName(QStringLiteral("CameraEmulator"));
        const auto overrideLogDirectory = qEnvironmentVariable("CAMERA_LOG_DIR");
        const auto logDirectory = overrideLogDirectory.isEmpty()
            ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("logs"))
            : overrideLogDirectory;
        try {
            camera::infra::Logging::initialize(logDirectory.toStdString());
            camera::infra::Logging::write(spdlog::level::info, "Main", "startup", "camera_emulator started");
        } catch (const std::exception& exception) {
            std::clog << "ERROR Main logging-init " << exception.what() << '\n';
        }
        MainWindow window;
        window.show();
        const int result = app.exec();
        camera::infra::Logging::write(spdlog::level::info, "Main", "shutdown", "camera_emulator exited");
        camera::infra::Logging::shutdown();
        return result;
    } catch (const std::exception& exception) {
        camera::infra::Logging::write(spdlog::level::critical, "Main", "unhandled-exception", exception.what());
        std::clog << "FATAL Main unhandled-exception " << exception.what() << '\n';
        camera::infra::Logging::shutdown();
        return 1;
    } catch (...) {
        camera::infra::Logging::write(spdlog::level::critical, "Main", "unhandled-exception", "unknown C++ exception");
        std::clog << "FATAL Main unhandled-exception unknown C++ exception\n";
        camera::infra::Logging::shutdown();
        return 1;
    }
}
