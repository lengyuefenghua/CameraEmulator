#include "app/qt/main_window.hpp"

#include "core/validation.hpp"
#include "infra/logging.hpp"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace {
void log_ui_error(const std::string& source, const QString& message) {
    camera::infra::Logging::write(spdlog::level::err, "MainWindow", source, message.toStdString());
}

void log_ui_warning(const std::string& source, const QString& message) {
    camera::infra::Logging::write(spdlog::level::warn, "MainWindow", source, message.toStdString());
}

QJsonObject config_to_json(const camera::core::CameraConfig& config) {
    QJsonObject object;
    object.insert("schemaVersion", 1);
    object.insert("id", QString::fromStdString(config.id));
    object.insert("vendor", QString::fromStdString(config.vendor));
    object.insert("model", QString::fromStdString(config.model));
    object.insert("serial", QString::fromStdString(config.serial));
    object.insert("version", QString::fromStdString(config.version));
    object.insert("manufacturerInfo", QString::fromStdString(config.manufacturerInfo));
    object.insert("ip", QString::fromStdString(config.ip.to_string()));
    object.insert("subnetMask", QString::fromStdString(config.subnetMask.to_string()));
    object.insert("gateway", QString::fromStdString(config.gateway.to_string()));
    object.insert("httpPort", static_cast<int>(config.httpPort));
    object.insert("gvspPort", static_cast<int>(config.gvspPort));
    object.insert("width", static_cast<qint64>(config.width));
    object.insert("height", static_cast<qint64>(config.height));
    object.insert("pixelFormat", static_cast<qint64>(config.pixelFormat));
    object.insert("packetSize", static_cast<qint64>(config.packetSize));
    object.insert("packetDelay", static_cast<qint64>(config.packetDelay));
    object.insert("heartbeatTimeoutMs", static_cast<qint64>(config.heartbeatTimeoutMs));
    object.insert("frameRate", static_cast<qint64>(config.frameRate));
    object.insert("imagePath", QString::fromStdString(config.imagePath));
    return object;
}

camera::core::CameraConfig config_from_json(const QJsonObject& object) {
    if (object.value("schemaVersion").toInt() != 1) throw std::runtime_error("unsupported config schemaVersion");
    camera::core::CameraConfig config;
    config.id = object.value("id").toString().toStdString();
    config.vendor = object.value("vendor").toString().toStdString();
    config.model = object.value("model").toString().toStdString();
    config.serial = object.value("serial").toString().toStdString();
    config.version = object.value("version").toString().toStdString();
    config.manufacturerInfo = object.value("manufacturerInfo").toString().toStdString();
    config.ip = camera::core::Ipv4Address::parse(object.value("ip").toString().toStdString());
    config.subnetMask = camera::core::Ipv4Address::parse(object.value("subnetMask").toString().toStdString());
    config.gateway = camera::core::Ipv4Address::parse(object.value("gateway").toString().toStdString());
    config.httpPort = static_cast<std::uint16_t>(object.value("httpPort").toInt());
    config.gvspPort = static_cast<std::uint16_t>(object.value("gvspPort").toInt());
    config.width = static_cast<std::uint32_t>(object.value("width").toInt());
    config.height = static_cast<std::uint32_t>(object.value("height").toInt());
    config.pixelFormat = static_cast<camera::core::PixelFormat>(object.value("pixelFormat").toInt());
    config.packetSize = static_cast<std::uint32_t>(object.value("packetSize").toInt());
    config.packetDelay = static_cast<std::uint32_t>(object.value("packetDelay").toInt());
    config.heartbeatTimeoutMs = static_cast<std::uint32_t>(object.value("heartbeatTimeoutMs").toInt());
    config.frameRate = static_cast<std::uint32_t>(object.value("frameRate").toInt());
    config.imagePath = object.value("imagePath").toString().toStdString();
    const auto validation = camera::core::validate(config);
    if (!validation.valid()) throw std::runtime_error(validation.errors.front());
    return config;
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("GigE Vision 相机模拟器"));
    resize(1100, 720);

    auto* toolbar = addToolBar(QStringLiteral("相机"));
    const auto addAction = [toolbar](const QString& label) { return toolbar->addAction(label); };
    auto* create = addAction(QStringLiteral("新建"));
    auto* edit = addAction(QStringLiteral("编辑"));
    auto* refreshProfilesAction = addAction(QStringLiteral("刷新列表"));
    auto* start = addAction(QStringLiteral("启动所选"));
    auto* stop = addAction(QStringLiteral("停止"));
    auto* xml = addAction(QStringLiteral("XML 预览"));
    auto* save = addAction(QStringLiteral("保存配置"));
    auto* load = addAction(QStringLiteral("载入配置"));
    connect(create, &QAction::triggered, this, &MainWindow::createCamera);
    connect(edit, &QAction::triggered, this, &MainWindow::editCamera);
    connect(refreshProfilesAction, &QAction::triggered, this, &MainWindow::refreshProfiles);
    connect(start, &QAction::triggered, this, &MainWindow::startCamera);
    connect(stop, &QAction::triggered, this, &MainWindow::stopCamera);
    connect(xml, &QAction::triggered, this, &MainWindow::showXml);
    connect(save, &QAction::triggered, this, &MainWindow::saveConfig);
    connect(load, &QAction::triggered, this, &MainWindow::loadConfig);

    auto* root = new QSplitter(this);
    cameraList_ = new QListWidget(root);
    cameraList_->setMinimumWidth(190);
    auto* center = new QWidget(root);
    auto* centerLayout = new QVBoxLayout(center);
    featureTree_ = new QTreeWidget(center);
    featureTree_->setHeaderLabels({QStringLiteral("特征"), QStringLiteral("当前值")});
    featureTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    featureTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    auto* editRow = new QWidget(center);
    auto* editLayout = new QFormLayout(editRow);
    valueEdit_ = new QLineEdit(editRow);
    auto* apply = new QPushButton(QStringLiteral("应用选中值"), editRow);
    editLayout->addRow(QStringLiteral("值"), valueEdit_);
    editLayout->addRow(apply);
    connect(apply, &QPushButton::clicked, this, &MainWindow::applyFeature);
    centerLayout->addWidget(featureTree_, 1);
    centerLayout->addWidget(editRow);

    auto* right = new QWidget(root);
    auto* rightLayout = new QVBoxLayout(right);
    profileDetails_ = new QLabel(QStringLiteral("已选配置：未选择"), right);
    profileDetails_->setWordWrap(true);
    stateLabel_ = new QLabel(QStringLiteral("状态：已停止"), right);
    peerLabel_ = new QLabel(QStringLiteral("Wintun：未启动"), right);
    peerLabel_->setWordWrap(true);
    statsLabel_ = new QLabel(QStringLiteral("帧率：0   总帧：0   丢帧：0"), right);
    diagnostics_ = new QPlainTextEdit(right);
    diagnostics_->setReadOnly(true);
    rightLayout->addWidget(profileDetails_);
    rightLayout->addWidget(stateLabel_);
    rightLayout->addWidget(peerLabel_);
    rightLayout->addWidget(statsLabel_);
    rightLayout->addWidget(new QLabel(QStringLiteral("诊断"), right));
    rightLayout->addWidget(diagnostics_, 1);
    root->addWidget(cameraList_);
    root->addWidget(center);
    root->addWidget(right);
    root->setStretchFactor(0, 1);
    root->setStretchFactor(1, 3);
    root->setStretchFactor(2, 1);
    setCentralWidget(root);
    statusBar()->showMessage(QStringLiteral("后端就绪 | 启动所选将创建专属 Wintun 虚拟适配器"));

    connect(featureTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current) {
        if (current) valueEdit_->setText(current->text(1));
    });
    connect(cameraList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*) {
        updateSelectedProfileDetails();
    });
    connect(&statusTimer_, &QTimer::timeout, this, &MainWindow::refreshStatus);
    statusTimer_.start(500);
    refreshProfiles();
}

void MainWindow::createCamera() { editCamera(); }

void MainWindow::editCamera() {
    QDialog dialog(this);
    dialog.setWindowTitle(engineClient_.backend().registers() ? QStringLiteral("编辑相机") : QStringLiteral("新建相机"));
    auto* form = new QFormLayout(&dialog);
    QLineEdit id(QString::fromStdString(config_.id), &dialog);
    QLineEdit ip(QString::fromStdString(config_.ip.to_string()), &dialog);
    QLineEdit vendor(QString::fromStdString(config_.vendor), &dialog);
    QLineEdit model(QString::fromStdString(config_.model), &dialog);
    QLineEdit serial(QString::fromStdString(config_.serial), &dialog);
    QLineEdit httpPort(QString::number(config_.httpPort), &dialog);
    QLineEdit streamPort(QString::number(config_.gvspPort), &dialog);
    QLineEdit width(QString::number(config_.width), &dialog);
    QLineEdit height(QString::number(config_.height), &dialog);
    QComboBox pixelFormat(&dialog);
    pixelFormat.addItem(QStringLiteral("Mono8"), static_cast<quint32>(camera::core::PixelFormat::Mono8));
    pixelFormat.addItem(QStringLiteral("RGB8"), static_cast<quint32>(camera::core::PixelFormat::RGB8));
    pixelFormat.setCurrentIndex(config_.pixelFormat == camera::core::PixelFormat::RGB8 ? 1 : 0);
    auto* image = new QWidget(&dialog);
    auto* imageLayout = new QHBoxLayout(image);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    auto* imageLabel = new QLabel(imagePath_.isEmpty() ? QStringLiteral("未选择") : imagePath_, image);
    auto* browse = new QPushButton(QStringLiteral("选择…"), image);
    imageLayout->addWidget(imageLabel, 1);
    imageLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, &dialog, [this, imageLabel] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("选择固定图像"), {}, QStringLiteral("Windows BMP (*.bmp)"));
        if (!path.isEmpty()) { imagePath_ = path; imageLabel->setText(path); }
    });
    form->addRow(QStringLiteral("设备 ID"), &id);
    form->addRow(QStringLiteral("IPv4 接口"), &ip);
    form->addRow(QStringLiteral("厂商"), &vendor);
    form->addRow(QStringLiteral("型号"), &model);
    form->addRow(QStringLiteral("序列号"), &serial);
    form->addRow(QStringLiteral("HTTP 端口"), &httpPort);
    form->addRow(QStringLiteral("GVSP 默认端口"), &streamPort);
    form->addRow(QStringLiteral("宽"), &width);
    form->addRow(QStringLiteral("高"), &height);
    form->addRow(QStringLiteral("像素格式"), &pixelFormat);
    form->addRow(QStringLiteral("固定图像源"), image);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        auto next = config_;
        next.id = id.text().toStdString();
        next.ip = camera::core::Ipv4Address::parse(ip.text().toStdString());
        next.vendor = vendor.text().toStdString();
        next.model = model.text().toStdString();
        next.serial = serial.text().toStdString();
        bool ok{};
        const auto parsePort = [&ok](const QLineEdit& edit) {
            const auto port = edit.text().toUInt(&ok);
            if (!ok || port == 0 || port > 65535) throw std::runtime_error("port must be from 1 to 65535");
            return static_cast<std::uint16_t>(port);
        };
        next.httpPort = parsePort(httpPort);
        next.gvspPort = parsePort(streamPort);
        next.width = width.text().toUInt(&ok);
        if (!ok) throw std::runtime_error("width must be an integer");
        next.height = height.text().toUInt(&ok);
        if (!ok) throw std::runtime_error("height must be an integer");
        next.pixelFormat = static_cast<camera::core::PixelFormat>(pixelFormat.currentData().toUInt());
        next.imagePath = imagePath_.toStdString();
        const auto validation = camera::core::validate(next);
        if (!validation.valid()) throw std::runtime_error(validation.errors.front());
        std::string error;
        if (!engineClient_.backend().update_config(next, error)) throw std::runtime_error(error);
        config_ = std::move(next);
        cameraList_->clear();
        cameraList_->addItem(QString::fromStdString(config_.id));
        populateFeatures();
        saveConfig();
        logOutcome(QStringLiteral("相机配置已更新。"));
    } catch (const std::exception& exception) {
        logOutcome(QStringLiteral("配置失败 camera=%1").arg(QString::fromStdString(config_.id)));
        QMessageBox::warning(this, QStringLiteral("配置无效"), QString::fromUtf8(exception.what()));
    }
}

void MainWindow::saveConfig() {
    if (!engineClient_.backend().registers()) return;
    const auto directory = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("virtual-cameras"));
    if (!QDir().mkpath(directory)) {
        const auto message = QStringLiteral("配置保存失败 camera=%1 path=%2")
            .arg(QString::fromStdString(config_.id), directory);
        logOutcome(message);
        log_ui_error("save-config", message);
        return;
    }
    const auto path = QDir(directory).filePath(QString::fromStdString(config_.id) + QStringLiteral(".json"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(config_to_json(config_)).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        const auto message = QStringLiteral("配置保存失败 camera=%1 path=%2").arg(QString::fromStdString(config_.id), path);
        logOutcome(message);
        log_ui_error("save-config", message);
        return;
    }
    configPath_ = path;
    refreshProfiles();
    for (int row = 0; row < cameraList_->count(); ++row) {
        const auto index = cameraList_->item(row)->data(Qt::UserRole).toInt();
        if (index >= 0 && static_cast<std::size_t>(index) < profiles_.size() && profiles_[static_cast<std::size_t>(index)].filePath == std::filesystem::path(path.toStdString())) {
            cameraList_->setCurrentRow(row);
            break;
        }
    }
    logOutcome(QStringLiteral("配置已保存 path=%1").arg(path));
}

void MainWindow::loadConfig() {
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("载入相机配置"), configPath_, QStringLiteral("JSON 配置 (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        const auto message = QStringLiteral("配置加载失败 path=%1").arg(path);
        logOutcome(message);
        log_ui_error("load-config", message);
        return;
    }
    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const auto message = QStringLiteral("配置加载失败 path=%1").arg(path);
        logOutcome(message);
        log_ui_error("load-config", message);
        return;
    }
    try {
        auto next = config_from_json(document.object());
        std::string error;
        if (!engineClient_.backend().update_config(next, error)) throw std::runtime_error(error);
        config_ = std::move(next);
        imagePath_ = QString::fromStdString(config_.imagePath);
        configPath_ = path;
        cameraList_->clear();
        cameraList_->addItem(QString::fromStdString(config_.id));
        populateFeatures();
        logOutcome(QStringLiteral("配置已载入。"));
    } catch (const std::exception& exception) {
        const auto message = QStringLiteral("配置加载失败 path=%1").arg(path);
        logOutcome(message);
        log_ui_error("load-config", message);
        QMessageBox::warning(this, QStringLiteral("配置加载失败"), QString::fromUtf8(exception.what()));
    }
}

void MainWindow::refreshProfiles() {
    const auto directory = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / "virtual-cameras";
    profiles_ = camera::core::CameraProfileCatalog::scan(directory);
    cameraList_->clear();
    for (const auto& profile : profiles_) {
        const auto name = QString::fromStdString(profile.filePath.filename().string());
        auto* item = new QListWidgetItem(profile.startable ? name : name + QStringLiteral("（不可启动）"), cameraList_);
        item->setData(Qt::UserRole, static_cast<int>(&profile - profiles_.data()));
        if (!profile.startable) {
            const auto reason = QString::fromStdString(profile.error);
            item->setToolTip(reason);
            log_ui_warning("profile-scan", QStringLiteral("配置不可启动 path=%1 reason=%2")
                .arg(QString::fromStdString(profile.filePath.string()), reason));
        }
    }
    if (!profiles_.empty()) {
        cameraList_->setCurrentRow(0);
        const auto& selected = profiles_.front();
        logOutcome(QStringLiteral("已选择配置 path=%1 status=%2")
            .arg(QString::fromStdString(selected.filePath.string()), selected.startable ? QStringLiteral("有效") : QString::fromStdString(selected.error)));
    }
    updateSelectedProfileDetails();
    const auto message = QStringLiteral("配置列表已刷新：%1 项，目录=%2")
        .arg(profiles_.size()).arg(QString::fromStdString(directory.string()));
    logOutcome(message);
}

void MainWindow::updateSelectedProfileDetails() {
    const auto* item = cameraList_->currentItem();
    if (!item) {
        profileDetails_->setText(QStringLiteral("已选配置：未选择"));
        return;
    }
    const auto index = item->data(Qt::UserRole).toInt();
    if (index < 0 || static_cast<std::size_t>(index) >= profiles_.size()) return;
    const auto& profile = profiles_[static_cast<std::size_t>(index)];
    if (!profile.startable) {
        profileDetails_->setText(QStringLiteral("已选配置：不可启动\n文件：%1\n原因：%2")
            .arg(QString::fromStdString(profile.filePath.filename().string()), QString::fromStdString(profile.error)));
        return;
    }
    const auto& config = profile.config;
    profileDetails_->setText(QStringLiteral("已选配置：%1\nIP：%2\n图像：%3\n分辨率：%4 × %5\n帧率：%6")
        .arg(QString::fromStdString(config.id), QString::fromStdString(config.ip.to_string()),
             QString::fromStdString(config.imagePath))
        .arg(config.width).arg(config.height).arg(config.frameRate));
}

void MainWindow::startCamera() {
    const auto* item = cameraList_->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("启动相机"), QStringLiteral("请先选择一份虚拟相机配置。"));
        return;
    }
    const auto index = item->data(Qt::UserRole).toInt();
    if (index < 0 || static_cast<std::size_t>(index) >= profiles_.size()) return;
    const auto& profile = profiles_[static_cast<std::size_t>(index)];
    if (!profile.startable) {
        const auto message = QStringLiteral("配置不可启动 path=%1 reason=%2")
            .arg(QString::fromStdString(profile.filePath.string()), QString::fromStdString(profile.error));
        logOutcome(message);
        log_ui_error("start-profile", message);
        QMessageBox::warning(this, QStringLiteral("配置不可启动"), QString::fromStdString(profile.error));
        return;
    }
    std::string error;
    if (!engineClient_.backend().switch_and_start(profile.config, error)) {
        const auto message = QStringLiteral("启动失败 camera=%1 reason=%2")
            .arg(QString::fromStdString(profile.config.id), QString::fromStdString(error));
        logOutcome(message);
        log_ui_error("start-profile", message);
        QMessageBox::critical(this, QStringLiteral("启动失败"), QString::fromStdString(error));
        return;
    }
    config_ = profile.config;
    imagePath_ = QString::fromStdString(config_.imagePath);
    populateFeatures();
    const auto peer = engineClient_.backend().peer_status();
    logOutcome(QStringLiteral("相机配置已启动：%1，适配器=%2，主机端=%3，相机端=%4")
        .arg(QString::fromStdString(config_.id), QString::fromStdString(peer.adapterName),
             QString::fromStdString(peer.hostAddress.to_string()), QString::fromStdString(peer.cameraAddress.to_string())));
    refreshStatus();
}

void MainWindow::stopCamera() {
    engineClient_.backend().stop_acquisition();
    engineClient_.backend().stop();
    logOutcome(QStringLiteral("相机服务已停止。"));
    refreshStatus();
}

void MainWindow::showXml() {
    const auto xml = QString::fromStdString(engineClient_.backend().xml_preview());
    if (xml.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("XML 预览"), QStringLiteral("请先创建相机配置。"));
        return;
    }
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("GenICam XML 预览"));
    dialog->resize(820, 600);
    auto* layout = new QVBoxLayout(dialog);
    auto* view = new QPlainTextEdit(dialog);
    view->setReadOnly(true);
    view->setPlainText(xml);
    layout->addWidget(view);
    dialog->show();
}

void MainWindow::applyFeature() {
    const auto* item = featureTree_->currentItem();
    if (!item) return;
    bool ok{};
    const auto value = valueEdit_->text().toUInt(&ok, 0);
    if (!ok) return;
    const auto address = static_cast<std::uint32_t>(item->data(0, Qt::UserRole).toUInt());
    std::string error;
    const auto peer = engineClient_.backend().config().ip.to_string();
    if (!engineClient_.backend().acquire_control(peer, error) || !engineClient_.backend().write_register(peer, address, value, error)) {
        const auto message = QStringLiteral("特征写入失败 camera=%1 reason=%2").arg(QString::fromStdString(config_.id), QString::fromStdString(error));
        logOutcome(message);
        log_ui_error("apply-feature", message);
        return;
    }
    featureTree_->currentItem()->setText(1, QString::number(value));
}

void MainWindow::refreshStatus() {
    const auto status = engineClient_.backend().status();
    stateLabel_->setText(status.state == camera::core::RuntimeState::Running
        ? QStringLiteral("状态：运行中") : QStringLiteral("状态：已停止"));
    const auto peer = engineClient_.backend().peer_status();
    if (peer.running) {
        peerLabel_->setText(QStringLiteral("Wintun：就绪\n适配器：%1\n主机端：%2  相机端：%3")
            .arg(QString::fromStdString(peer.adapterName), QString::fromStdString(peer.hostAddress.to_string()),
                 QString::fromStdString(peer.cameraAddress.to_string())));
    } else if (!peer.error.empty()) {
        peerLabel_->setText(QStringLiteral("Wintun：启动/运行失败\n原因：%1").arg(QString::fromStdString(peer.error)));
    } else {
        peerLabel_->setText(QStringLiteral("Wintun：未启动"));
    }
    const auto deltaFrames = status.frameCount - lastFrameCount_;
    lastFrameCount_ = status.frameCount;
    statsLabel_->setText(QStringLiteral("帧增量/秒：%1   总帧：%2   丢帧：%3")
        .arg(deltaFrames * 2).arg(status.frameCount).arg(status.droppedFrames));
}

void MainWindow::populateFeatures() {
    featureTree_->clear();
    for (const auto& feature : engineClient_.backend().features()) {
        auto* item = new QTreeWidgetItem(featureTree_);
        item->setText(0, QString::fromStdString(feature.displayName.empty() ? feature.name : feature.displayName));
        item->setText(1, QString::number(feature.initialValue));
        item->setData(0, Qt::UserRole, feature.address);
        item->setToolTip(0, QString::fromStdString(feature.category));
    }
}

void MainWindow::logOutcome(const QString& text) {
    diagnostics_->appendPlainText(text);
    camera::infra::Logging::write(spdlog::level::info, "MainWindow", "outcome", text.toStdString());
}
