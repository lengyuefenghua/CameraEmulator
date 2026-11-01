#pragma once

#include "core/camera_model.hpp"
#include "core/camera_profile_catalog.hpp"
#include "app/qt/engine_client.hpp"

#include <QMainWindow>
#include <QTimer>
#include <QString>

#include <cstdint>
#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    camera::app::EngineClient engineClient_;
    camera::core::CameraConfig config_;
    QString configPath_;
    QString imagePath_;
    std::vector<camera::core::CameraProfile> profiles_;
    std::uint64_t lastFrameCount_{};
    QListWidget* cameraList_{};
    QTreeWidget* featureTree_{};
    QLineEdit* valueEdit_{};
    QLabel* profileDetails_{};
    QLabel* stateLabel_{};
    QLabel* peerLabel_{};
    QLabel* statsLabel_{};
    QPlainTextEdit* diagnostics_{};
    QTimer statusTimer_;

    void createCamera();
    void editCamera();
    void saveConfig();
    void loadConfig();
    void refreshProfiles();
    void updateSelectedProfileDetails();
    void startCamera();
    void stopCamera();
    void showXml();
    void applyFeature();
    void refreshStatus();
    void populateFeatures();
    void logOutcome(const QString& text);
};
