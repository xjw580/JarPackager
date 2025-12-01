/**************************************************************************

Author:肖嘉威

Version:1.0.0

Date:2025/9/13 9:51

Description:

**************************************************************************/

#include "jarpackager.h"

#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QMessageBox>
#include <QProcess>
#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutex>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtGui/QCloseEvent>
#include <QProgressDialog>
#include <attach.h>
#include <modify.h>
#include <Windows.h>
#include <ShlObj.h>

#include "jarcommon.h"
#include "ui_jarpackager.h"

import std;

// 静态实例指针初始化
JarPackagerWindow *JarPackagerWindow::instance = nullptr;
static QMutex logMutex;
static bool loggingEnabled = false;

QJsonObject PackageConfig::toJson() const {
    QJsonObject obj;
    obj["jarPath"] = jarPath;
    obj["outputPath"] = outputPath;
    obj["jvmArgs"] = QJsonArray::fromStringList(jvmArgs);
    obj["programArgs"] = QJsonArray::fromStringList(programArgs);
    obj["javaPath"] = javaPath;
    obj["jarExtractPath"] = jarExtractPath;
    obj["launchMode"] = launchMode;
    obj["javaVersion"] = QString::number(javaVersion); // 防止 JS 数字溢出
    obj["mainClass"] = mainClass;
    obj["enableSplash"] = enableSplash;
    obj["splashImagePath"] = splashImagePath;
    obj["splashShowProgress"] = splashShowProgress;
    obj["splashShowProgressText"] = splashShowProgressText;
    obj["launchTime"] = QString::number(launchTime);
    obj["splashProgramName"] = splashProgramName;
    obj["splashProgramVersion"] = splashProgramVersion;
    obj["iconPath"] = iconPath;
    obj["showConsole"] = showConsole;
    obj["requireAdmin"] = requireAdmin;
    obj["externalExePath"] = externalExePath;
    return obj;
}

void PackageConfig::fromJson(const QJsonObject &obj) {
    jarPath = obj.value("jarPath").toString();
    outputPath = obj.value("outputPath").toString();
    QJsonArray jvmArray = obj["jvmArgs"].toArray();
    jvmArgs.clear();
    for (const auto &value: jvmArray) {
        jvmArgs.append(value.toString());
    }
    QJsonArray progArray = obj["programArgs"].toArray();
    programArgs.clear();
    for (const auto &value: progArray) {
        programArgs.append(value.toString());
    }
    javaPath = obj.value("javaPath").toString();
    jarExtractPath = obj.value("jarExtractPath").toString();
    launchMode = obj.value("launchMode").toInt(0);
    javaVersion = obj.value("javaVersion").toString().toLongLong();
    mainClass = obj.value("mainClass").toString();
    enableSplash = obj.value("enableSplash").toBool(false);
    splashImagePath = obj.value("splashImagePath").toString();
    splashShowProgress = obj.value("splashShowProgress").toBool();
    splashShowProgressText = obj.value("splashShowProgressText").toBool();
    launchTime = obj.value("launchTime").toString().toInt();
    splashProgramName = obj.value("splashProgramName").toString();
    splashProgramVersion = obj.value("splashProgramVersion").toString();
    iconPath = obj.value("iconPath").toString();
    showConsole = obj.value("showConsole").toBool(false);
    requireAdmin = obj.value("requireAdmin").toBool(false);
    externalExePath = obj.value("externalExePath").toString();
}

QJsonObject SoftConfig::toJson() const {
    QJsonObject obj;
    obj["lastSoftConfigPath"] = lastSoftConfigPath;
    return obj;
}

void SoftConfig::fromJson(const QJsonObject &obj) {
    lastSoftConfigPath = obj["lastSoftConfigPath"].toString();
}


// Packager 实现
std::expected<bool, QString> Packager::packageJar(const Config &config) {
    // 读取JAR文件
    QFile jarFile(config.jarPath);
    if (!jarFile.open(QIODevice::ReadOnly)) {
        return std::unexpected(QString("无法打开JAR文件: %1").arg(jarFile.errorString()));
    }
    const QByteArray jarData = jarFile.readAll();
    jarFile.close();

    // 准备字符串数据
    const QByteArray mainClassBytes = config.mainClass.toUtf8();
    const QByteArray jvmArgsBytes = config.jvmArgs.join('\n').toUtf8();
    const QByteArray programArgsBytes = config.programArgs.join('\n').toUtf8();
    const QByteArray javaPathBytes = config.javaPath.toUtf8();
    const QByteArray jarExtractPathBytes = config.jarExtractPath.toUtf8();
    const QByteArray splashProgramNameBytes = config.splashProgramName.toUtf8();
    const QByteArray splashProgramVersionBytes = config.splashProgramVersion.toUtf8();

    // 写入输出文件
    QFile outFile(config.outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        return std::unexpected(QString("无法创建输出文件: %1").arg(outFile.errorString()));
    }

    // 写入EXE数据
    outFile.write(config.exeData);
    outFile.close();
    if (auto modifyRes = modifyExe(config.outputPath, config.iconPath, config.showConsole, config.requireAdmin);
        !modifyRes) {
        return std::unexpected(QString("修改exe失败: %1").arg(modifyRes.error()));
    }
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    const qint64 exeSize = outFile.size();
    outFile.seek(exeSize);

    // 写入JAR数据
    outFile.write(jarData);

    QByteArray pngData;
    if (!config.splashImagePath.isEmpty()) {
        // 读取图片
        const QImage image(config.splashImagePath);
        if (image.isNull()) {
            return std::unexpected(QString("无法打开图片: %1").arg(config.splashImagePath));
        }
        QBuffer buffer(&pngData);
        if (!buffer.open(QIODevice::WriteOnly)) {
            return std::unexpected(QString("无法打开缓冲区进行写入"));
        }
        if (!image.save(&buffer, "PNG")) {
            return std::unexpected(QString("图片转码失败: %1").arg(config.splashImagePath));
        }
        buffer.close();
    }
    // 写入图片数据
    outFile.write(pngData);
    // 写入字符串数据
    outFile.write(mainClassBytes);
    outFile.write(jvmArgsBytes);
    outFile.write(programArgsBytes);
    outFile.write(javaPathBytes);
    outFile.write(jarExtractPathBytes);
    outFile.write(splashProgramNameBytes);
    outFile.write(splashProgramVersionBytes);

    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // 创建Footer结构
    const JarCommon::JarFooter footer{
        JarCommon::JAR_MAGIC,
        static_cast<unsigned long long>(exeSize),
        static_cast<unsigned long long>(jarData.size()),
        static_cast<unsigned long long>(pngData.size()),
        config.splashShowProgress,
        config.splashShowProgressText,
        config.launchTime,
        static_cast<unsigned long long>(timestamp),
        config.javaVersion,
        static_cast<unsigned int>(mainClassBytes.size()),
        static_cast<unsigned int>(jvmArgsBytes.size()),
        static_cast<unsigned int>(programArgsBytes.size()),
        static_cast<unsigned int>(javaPathBytes.size()),
        static_cast<unsigned int>(jarExtractPathBytes.size()),
        static_cast<unsigned int>(splashProgramNameBytes.size()),
        static_cast<unsigned int>(splashProgramVersionBytes.size()),
        config.launchMode,
    };

    // 写入Footer结构体
    outFile.write(reinterpret_cast<const char *>(&footer), sizeof(JarCommon::JarFooter));

    outFile.close();
    return true;
}

std::expected<bool, QString> Packager::extractJarInfo(const QString &jarPath, PackageConfig &jarInfo) {
    QFile file(jarPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::unexpected{QString("无法打开文件: %1").arg(file.errorString())};
    }

    const qint64 fileSize = file.size();
    if (fileSize < sizeof(JarCommon::JarFooter)) {
        return std::unexpected{"文件太小，不包含有效的JAR信息"};
    }

    // 读取Footer结构体
    file.seek(fileSize - sizeof(JarCommon::JarFooter));
    JarCommon::JarFooter footer{};
    if (file.read(reinterpret_cast<char *>(&footer), sizeof(JarCommon::JarFooter)) != sizeof(JarCommon::JarFooter)) {
        return std::unexpected{"读取Footer失败"};
    }

    // 验证魔数
    if (footer.magic != JarCommon::JAR_MAGIC) {
        return std::unexpected{QString("无效的魔数: 0x%1").arg(footer.magic, 8, 16, QChar('0'))};
    }

    // 计算字符串数据的位置
    const qint64 stringsOffset = fileSize - sizeof(JarCommon::JarFooter) - footer.mainClassLength -
                                 footer.jvmArgsLength - footer.programArgsLength -
                                 footer.javaPathLength - footer.jarExtractPathLength;

    file.seek(stringsOffset);

    // 读取字符串数据
    const QByteArray mainClassData = file.read(footer.mainClassLength);
    const QByteArray jvmArgsData = file.read(footer.jvmArgsLength);
    const QByteArray programArgsData = file.read(footer.programArgsLength);
    const QByteArray javaPathData = file.read(footer.javaPathLength);
    const QByteArray jarExtractPathData = file.read(footer.jarExtractPathLength);

    file.close();

    // 填充JarInfo
    jarInfo.javaVersion = footer.javaVersion;
    jarInfo.mainClass = QString::fromUtf8(mainClassData);
    jarInfo.jvmArgs = QString::fromUtf8(jvmArgsData).split('\n', Qt::SkipEmptyParts);
    jarInfo.programArgs = QString::fromUtf8(programArgsData).split('\n', Qt::SkipEmptyParts);
    jarInfo.javaPath = QString::fromUtf8(javaPathData);
    jarInfo.jarExtractPath = QString::fromUtf8(jarExtractPathData);
    jarInfo.launchMode = static_cast<int>(footer.launchMode);

    return true;
}

std::expected<bool, QString> Packager::modifyExe(const QString &exePath, const QString &iconPath,
                                                 const bool showConsole, const bool requireAdmin) {
    QFile exeFile(exePath);
    if (!exeFile.open(QIODevice::ReadWrite)) {
        return std::unexpected(QString("无法打开EXE文件: %1").arg(exeFile.errorString()));
    }
    exeFile.close();

    const auto exePathW = exePath.toStdWString();
    PEModifier modifier(exePathW);
    modifier.loadFile();

    if (const auto &res = modifier.getExecutionLevel(); res) {
        if (requireAdmin && res.value() == ExecutionLevel::AsInvoker) {
            if (auto setRes = modifier.setExecutionLevel(ExecutionLevel::RequireAdmin); !setRes) {
                return std::unexpected{QString::fromStdWString(setRes.error())};
            }
        } else if (!requireAdmin && res.value() == ExecutionLevel::RequireAdmin) {
            if (auto setRes = modifier.setExecutionLevel(ExecutionLevel::AsInvoker); !setRes) {
                return std::unexpected{QString::fromStdWString(setRes.error())};
            }
        }
    } else {
        return std::unexpected{QString::fromStdWString(res.error())};
    }

    if (!iconPath.isEmpty()) {
        QFile iconFile(iconPath);
        if (!iconFile.open(QIODevice::ReadOnly)) {
            return std::unexpected(QString("无法打开图标文件: %1").arg(iconFile.errorString()));
        }
        iconFile.close();
        if (auto updateIcoRes = modifier.setIcon(iconPath.toStdWString().c_str()); !updateIcoRes) {
            return std::unexpected(QString("无法更新图标: %1").arg(QString::fromStdWString(updateIcoRes.error())));
        }
    }

    if (showConsole) {
        if (auto res = modifier.setSubsystem(IMAGE_SUBSYSTEM_WINDOWS_CUI); !res) {
            return std::unexpected{QString::fromStdWString(res.error())};
        }
    } else {
        if (auto res = modifier.setSubsystem(IMAGE_SUBSYSTEM_WINDOWS_GUI); !res) {
            return std::unexpected{QString::fromStdWString(res.error())};
        }
    }

    return true;
}

// JarPackagerWindow 实现
JarPackagerWindow::JarPackagerWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::JarPackagerWindow) {
    ui->setupUi(this);

    // 设置静态实例指针
    instance = this;

    // 设置日志系统
    setupLogging();

    // 初始化状态栏
    updateStatus("就绪");

    // 初始化启动模式
    modeMap[0] = ui->modeJava;
    modeMap[1] = ui->modeJvm;
    ui->modeButtonGroup->setId(modeMap[0], 0);
    ui->modeButtonGroup->setId(modeMap[1], 1);
    constexpr int mode_index = 0;
    const auto &radio_button = modeMap[mode_index];
    radio_button->setChecked(true);
    on_modeButtonGroup_idToggled(mode_index, radio_button->isChecked());

    // 初始化java版本
    const auto &versionMap = JarCommon::JAVA_VERSION_MAP;
    QStringList keys;
    for (const auto &key: versionMap | std::views::keys) {
        keys.append(QString::fromStdString(key));
    }

    std::ranges::sort(keys, [&](const QString &a, const QString &b) {
        return versionMap.at(a.toStdString()) < versionMap.at(b.toStdString());
    });
    ui->javaVersionComboBox->addItems(keys);
    ui->javaVersionComboBox->setCurrentText(keys.last());

    // 初始化启动页配置
    on_enablSplashCheckBox_stateChanged(ui->enablSplashCheckBox->isChecked() ? Qt::Checked : Qt::Unchecked);
    ui->launchTimeEdit->setValidator(new QIntValidator(0, 999999, ui->launchTimeEdit));
    ui->splashView->setScene(new QGraphicsScene(this));

    // 连接文本改变信号，用于标记配置已改变
    // 基本设置
    connect(ui->jarEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->outEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    // 运行设置
    connect(ui->jvmArgsEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->progArgsEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->javaPathEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->jarExtractPathEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->modeJava, &QRadioButton::toggled, [this]() { configChanged = true; });
    connect(ui->javaVersionComboBox, &QComboBox::currentTextChanged, [this]() { configChanged = true; });
    connect(ui->mainClassEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    // 启动页设置
    connect(ui->enablSplashCheckBox, &QCheckBox::checkStateChanged, [this]() { configChanged = true; });
    connect(ui->splashImageEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->splashNameEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->splashVersionEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->splashShowProgressCheckBox, &QCheckBox::checkStateChanged, [this]() { configChanged = true; });
    connect(ui->splashShowProgresstTextCheckBox, &QCheckBox::checkStateChanged, [this]() { configChanged = true; });
    connect(ui->launchTimeEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    // exe设置
    connect(ui->iconPathEdit, &QLineEdit::textChanged, [this]() { configChanged = true; });
    connect(ui->showConsoleCheckBox, &QCheckBox::checkStateChanged, [this]() { configChanged = true; });
    connect(ui->requireAdminCheckBox, &QCheckBox::checkStateChanged, [this]() { configChanged = true; });

    if (const auto &args = QCoreApplication::arguments(); args.size() > 1) {
        ui->jarEdit->setText(args.at(1));
    }
    const auto softConfigPath = QDir::current().filePath(softConfigName);
    if (QFile(softConfigPath).exists()) {
        loadSoftConfig(softConfigPath);
        if (QFile(currentConfigPath).exists()) {
            loadPackageConfig(currentConfigPath);
        }
    }
}

JarPackagerWindow::~JarPackagerWindow() {
    // 重置消息处理器
    loggingEnabled = false;
    qInstallMessageHandler(nullptr);
    instance = nullptr;
    delete ui;
}

void JarPackagerWindow::setupLogging() {
    // 安装自定义消息处理器
    qInstallMessageHandler(messageHandler);
    loggingEnabled = true;

    // 设置infoBox的初始内容
    ui->infoBox->setPlainText("");
}

void JarPackagerWindow::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // 防止递归调用
    QMutexLocker locker(&logMutex);

    if (!loggingEnabled || !instance || !instance->ui || !instance->ui->infoBox) {
        // 如果日志被禁用或实例不存在，输出到控制台
        std::cout << msg.toStdString() << std::endl;
        return;
    }

    QString typeStr;
    switch (type) {
        case QtDebugMsg:
            typeStr = "[DEBUG]";
            break;
        case QtInfoMsg:
            typeStr = "[INFO]";
            break;
        case QtWarningMsg:
            typeStr = "[WARN]";
            break;
        case QtCriticalMsg:
            typeStr = "[ERROR]";
            break;
        case QtFatalMsg:
            typeStr = "[FATAL]";
            break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMsg = QString("%1 %2 %3").arg(timestamp, typeStr, msg);

    // 直接在主线程中更新（如果在主线程）或使用队列调用
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        instance->appendLogMessage(formattedMsg);
    } else {
        QMetaObject::invokeMethod(instance, "appendLogMessage", Qt::QueuedConnection, Q_ARG(QString, formattedMsg));
    }
}

void JarPackagerWindow::appendLogMessage(const QString &message) {
    // 暂时禁用日志以避免递归
    loggingEnabled = false;

    // 在主线程中安全地更新UI
    ui->infoBox->append(message);

    // 自动滚动到底部
    QTextCursor cursor = ui->infoBox->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->infoBox->setTextCursor(cursor);

    // 限制日志行数
    static int lineCount = 0;
    lineCount++;
    if (lineCount > 500) {
        QString allText = ui->infoBox->toPlainText();
        QStringList lines = allText.split('\n');
        if (lines.size() > 300) {
            lines = lines.mid(lines.size() - 300);
            ui->infoBox->setPlainText(lines.join('\n'));
            lineCount = 300;
        }
    }

    // 重新启用日志
    loggingEnabled = true;
}

bool JarPackagerWindow::openAndSelectFile(const QString &path) {
    QString native = QDir::toNativeSeparators(path);
    std::wstring wpath = native.toStdWString();

    PIDLIST_ABSOLUTE pidl = nullptr;
    HRESULT hr = SHParseDisplayName(wpath.c_str(), nullptr, &pidl, 0, nullptr);
    if (FAILED(hr))
        return false;

    // 选中 1 个文件
    hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);

    CoTaskMemFree(pidl);
    return SUCCEEDED(hr);
}

void JarPackagerWindow::on_enablSplashCheckBox_stateChanged(const int state) {
    const auto disable = state != Qt::Checked;
    for (auto *child: ui->splashImageContentWidget->findChildren<QWidget *>()) {
        child->setDisabled(disable);
    }
}

void JarPackagerWindow::on_jarBtn_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "选择JAR文件", "", "JAR文件 (*.jar)");
    if (!fileName.isEmpty()) {
        ui->jarEdit->setText(fileName);
    }
}

void JarPackagerWindow::on_outBtn_clicked() {
    const QString &out_file =
            QFileDialog::getSaveFileName(this, "选择输出文件", "", "可执行文件 (*.exe);;可执行文件 (*.exe)");
    if (!out_file.isEmpty()) {
        ui->outEdit->setText(out_file);
    }
}

void JarPackagerWindow::on_javaPathBtn_clicked() {
    if (const QString dir = QFileDialog::getExistingDirectory(this, ""); !dir.isEmpty()) {
        auto java_dir = QDir(dir);
        auto java_path = java_dir.absolutePath();
        if (java_path.endsWith(QDir::separator())) {
            java_path.chop(1);
        }
        if (const auto bin_name = "bin"; !java_path.endsWith(bin_name, Qt::CaseInsensitive)) {
            java_path = QDir(java_path).filePath(bin_name);
        }
        java_dir = QDir(java_path);
        if (const auto &java_exe_path = java_dir.filePath(QString("%1").arg(JarCommon::JAVA_EXE_NAME));
            !QFile::exists(java_exe_path)) {
            qWarning() << QString("%1不存在").arg(java_exe_path);
        }
        bool existJVMDll = true;

        if (const auto &jvm_dll_path = java_dir.filePath(QString("server/%1").arg(JarCommon::JVM_DLL_NAME));
            !QFile::exists(jvm_dll_path)) {
            qWarning() << QString("%1不存在").arg(jvm_dll_path);
            existJVMDll = false;
        }
        if (const auto &jvm_dll_path = java_dir.filePath(QString("client/%1").arg(JarCommon::JVM_DLL_NAME));
            !existJVMDll && !QFile::exists(jvm_dll_path)) {
            qWarning() << QString("%1不存在").arg(jvm_dll_path);
            existJVMDll = false;
        }
        if (!existJVMDll) {
            qWarning() << "未找到jvm.dll";
        }
        ui->javaPathEdit->setText(java_path);
    }
}

void JarPackagerWindow::on_loadConfigBtn_clicked() { on_actionLoadConfig_triggered(); }

void JarPackagerWindow::on_saveConfigBtn_clicked() { on_actionSaveConfig_triggered(); }

void JarPackagerWindow::on_packageBtn_clicked() {
    const QString jarPath = ui->jarEdit->text().trimmed();
    const bool enableSplash = ui->enablSplashCheckBox->isChecked();
    const QString splashImagePath = enableSplash ? ui->splashImageEdit->text().trimmed() : QString();
    const bool splashShowProgress = ui->splashShowProgressCheckBox->isChecked();
    const bool splashShowProgressText = ui->splashShowProgresstTextCheckBox->isChecked();
    const int launchTime = ui->launchTimeEdit->text().trimmed().toInt();
    const QString splashProgramName = enableSplash ? ui->splashNameEdit->text().trimmed() : QString();
    const QString splashProgramVersion = enableSplash ? ui->splashVersionEdit->text().trimmed() : QString();
    const QString outputPath = ui->outEdit->text().trimmed();
    const QString mainClass = ui->mainClassEdit->text().trimmed();
    const QString javaVersion = ui->javaVersionComboBox->currentText();
    const QString jarExtractPath = ui->jarExtractPathEdit->text().trimmed();
    const QString iconPath = ui->iconPathEdit->text().trimmed();
    const auto showConsole = ui->showConsoleCheckBox->isChecked();
    const auto requireAdmin = ui->requireAdminCheckBox->isChecked();


    qInfo() << "开始验证打包参数...";

    // 验证基本输入
    if (jarPath.isEmpty() || outputPath.isEmpty()) {
        const auto log = QString("请填写必要的路径信息（EXE路径、JAR路径、输出路径）");
        qWarning() << log;
        QMessageBox::critical(this, "错误", log);
        return;
    }

    // 验证direct_jvm模式下的主类
    if (ui->modeJvm->isChecked() && (mainClass.isEmpty() || javaVersion.isEmpty())) {
        const auto log = QString("%1模式需要填写主类和Java版本").arg(ui->modeJvm->text());
        qWarning() << log;
        QMessageBox::warning(this, "错误", log);
        ui->mainClassEdit->setFocus();
        return;
    }

    // 检查文件是否存在
    if (enableSplash && !splashImagePath.isEmpty() && !QFile::exists(splashImagePath)) {
        const auto log = QString("启动页图片不存在: %1").arg(splashImagePath);
        qWarning() << log;
        QMessageBox::critical(this, "错误", log);
        return;
    }

    if (!QFile::exists(jarPath)) {
        const auto log = QString("JAR文件不存在: %1").arg(jarPath);
        qWarning() << log;
        QMessageBox::critical(this, "错误", log);
        return;
    }

    QStringList jvmArgs;
    if (const QString jvmArgsText = ui->jvmArgsEdit->text().trimmed(); !jvmArgsText.isEmpty()) {
        jvmArgs = jvmArgsText.split(";", Qt::SkipEmptyParts);
        for (QString &arg: jvmArgs) {
            arg = arg.trimmed();
        }
    }

    QStringList progArgs;
    if (const QString progArgsText = ui->progArgsEdit->text().trimmed(); !progArgsText.isEmpty()) {
        progArgs = progArgsText.split(";", Qt::SkipEmptyParts);
        for (QString &arg: progArgs) {
            arg = arg.trimmed();
        }
    }

    const JarCommon::LaunchMode launchMode =
            ui->modeJvm->isChecked() ? JarCommon::LaunchMode::DirectJVM : JarCommon::LaunchMode::JavaExe;
    const QString javaPath = ui->javaPathEdit->text().trimmed();

    qInfo() << QString("启动模式: %1")
            .arg(ui->modeJvm->isChecked() ? ui->modeJvm->text().trimmed() : ui->modeJava->text().trimmed());
    if (!jvmArgs.isEmpty()) {
        qInfo() << QString("JVM参数: %1").arg(jvmArgs.join(" "));
    }
    if (!progArgs.isEmpty()) {
        qInfo() << QString("程序参数: %1").arg(progArgs.join(" "));
    }

    // 开始打包
    updateStatus("正在打包...");
    const auto &ver = javaVersion.toStdString();
    const unsigned int version = JarCommon::JAVA_VERSION_MAP.contains(ver) ? JarCommon::JAVA_VERSION_MAP.at(ver) : 0;
    const auto readAttachRes = Attach::readAttachedExe(QCoreApplication::applicationFilePath().toStdWString());
    if (!readAttachRes) {
        const auto error = QString::fromStdWString(readAttachRes.error());
        qWarning() << "获取当前程序的附加exe失败, " << error;
        QMessageBox::critical(this, "获取附加exe失败", error);
        return;
    }

    auto byte = readAttachRes.value();
    auto configP = std::make_shared<Packager::Config>(
        QByteArray(reinterpret_cast<const char *>(byte.data()), static_cast<int>(byte.size())), jarPath,
        splashImagePath, splashShowProgress, splashShowProgressText, launchTime, version, outputPath, mainClass,
        jvmArgs, progArgs, javaPath, jarExtractPath, splashProgramName, splashProgramVersion, launchMode, iconPath,
        showConsole, requireAdmin);
    qInfo() << "开始打包...";

    auto *dialog = new QProgressDialog("打包中...", QString(), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setCancelButton(nullptr);
    dialog->show();

    std::thread t([=] {
        const auto res = Packager::packageJar(*configP);

        QMetaObject::invokeMethod(this, [=] {
            dialog->close();
            dialog->deleteLater();

            if (res) {
                qInfo() << "✓ 打包完成!";
                qInfo() << QString("输出文件: %1").arg(configP->outputPath);
                updateStatus("打包完成");

                if (QMessageBox::question(this, "打包完成", "是否打开输出目录？",
                                          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                    openAndSelectFile(configP->outputPath);
                }
            } else {
                qWarning() << QString("✗ 打包失败: %1").arg(res.error());
                updateStatus("打包失败");
                QMessageBox::critical(this, "打包失败", res.error());
            }
        }, Qt::QueuedConnection);
    });
    t.detach();
}

void JarPackagerWindow::on_loadExeBtn_clicked() {
    const QString exePath = QFileDialog::getOpenFileName(
        this, "选择EXE文件", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "可执行文件 (*.exe)");
    if (!exePath.isEmpty()) {
        ui->externalExePathEdit->setText(exePath);
    }
}

void JarPackagerWindow::on_modifyExeBtn_clicked() {
    if (const QString externalExePath = ui->externalExePathEdit->text().trimmed(); !externalExePath.isEmpty()) {
        const QString iconPath = ui->iconPathEdit->text().trimmed();
        const auto showConsole = ui->showConsoleCheckBox->checkState() == Qt::CheckState::Checked;
        const auto requireAdmin = ui->requireAdminCheckBox->checkState() == Qt::CheckState::Checked;
        if (auto res = Packager::modifyExe(externalExePath, iconPath, showConsole, requireAdmin); res) {
            qInfo() << "修改exe成功";
            const int ret =
                    QMessageBox::question(this, "修改exe成功", "是否打开目录？", QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                openAndSelectFile(externalExePath);
            }
        } else {
            qWarning() << "修改exe失败, " << res.error();
            QMessageBox::critical(this, "修改exe失败", res.error());
        }
    }
}

void JarPackagerWindow::on_attachExeAction_triggered() {
    if (const QString attachExePath = QFileDialog::getOpenFileName(this, "选择EXE文件", "", "可执行文件 (*.exe)");
        !attachExePath.isEmpty()) {
        if (const auto res = Attach::attachExeToCurrent(std::filesystem::path(attachExePath.toStdWString())); res) {
            const auto outPath = QString::fromStdWString(res.value());
            qInfo() << "完成生成附加 EXE:" << outPath;

            const int ret =
                    QMessageBox::question(this, "附加完成", "是否重启为新的exe？", QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                QProcess::startDetached(outPath);
                close();
            }
        } else {
            const auto error = QString::fromStdWString(res.error());
            qWarning() << QString("附加失败, %1").arg(error);
            QMessageBox::critical(this, "附加失败", error);
        }
    }
}

void JarPackagerWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (ui->splashView->scene())
        ui->splashView->fitInView(ui->splashView->scene()->sceneRect(), Qt::KeepAspectRatio);
}

void JarPackagerWindow::on_splashImageBtn_clicked() {
    QStringList formats;
    for (const QByteArray &fmt: QImageReader::supportedImageFormats())
        formats << "*." + QString(fmt);

    const QString filter = "图片文件 (" + formats.join(" ") + ")";
    const QString imageFilePath = QFileDialog::getOpenFileName(
        this, "选择启动页图片", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), filter);
    if (!imageFilePath.isEmpty()) {
        ui->splashImageEdit->setText(imageFilePath);
        const QPixmap pixmap(imageFilePath);
        if (pixmap.isNull()) {
            qWarning() << "启动页图片无效, " << imageFilePath;
            return;
        }
        ui->splashView->scene()->clear();
        ui->splashView->scene()->addPixmap(pixmap);
        ui->splashView->scene()->setSceneRect(pixmap.rect());
        ui->splashView->fitInView(ui->splashView->scene()->sceneRect(), Qt::KeepAspectRatio);
    }
}

void JarPackagerWindow::on_iconBtn_clicked() {
    const QString iconFilePath = QFileDialog::getOpenFileName(
        this, "选择图标文件", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "图标文件 (*.icon *.ico)");
    if (!iconFilePath.isEmpty()) {
        ui->iconPathEdit->setText(iconFilePath);
        const QPixmap pixmap(iconFilePath);
        if (pixmap.isNull()) {
            qWarning() << "图标无效, " << iconFilePath;
            return;
        }
        ui->iconView->setPixmap(pixmap.scaled(ui->iconView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}


void JarPackagerWindow::on_modeButtonGroup_idToggled(const int id, const bool checked) {
    if (!checked)
        return;
    const bool enable = modeMap[id] == ui->modeJvm;
    ui->mainClassLabel->setEnabled(enable);
    ui->mainClassEdit->setEnabled(enable);
    ui->javaVersionLabel->setEnabled(enable);
    ui->javaVersionComboBox->setEnabled(enable);
}

void JarPackagerWindow::on_actionLoadConfig_triggered() {
    const QString fileName = QFileDialog::getOpenFileName(
        this, "选择配置文件", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON配置 (*.json)");
    if (!fileName.isEmpty()) {
        loadPackageConfig(fileName);
        saveSoftConfig(QDir::current().filePath(softConfigName));
    }
}

void JarPackagerWindow::on_actionSaveConfig_triggered() {
    QString fileName;
    if (currentConfigPath.isEmpty()) {
        fileName = QFileDialog::getSaveFileName(this, "保存配置文件",
                                                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                                                "/jarpackager_config.json",
                                                "JSON配置 (*.json)");
    } else {
        fileName = currentConfigPath;
    }

    if (!fileName.isEmpty()) {
        savePackageConfig(fileName);
        saveSoftConfig(QDir::current().filePath(softConfigName));
    }
}

void JarPackagerWindow::on_actionExit_triggered() {
    qInfo() << "用户请求退出程序";
    close();
}

void JarPackagerWindow::on_actionAbout_triggered() {
    QMessageBox::about(this, "关于 JAR Packager",
                       "<h3>JAR Packager v1.0</h3>"
                       "<p>一个用于将JAR文件打包到EXE文件中的工具</p>"
                       "<p>基于 Qt6 C++ 开发</p>"
                       "<p><b>功能特点:</b></p>"
                       "<ul>"
                       "<li>将JAR文件嵌入到EXE中</li>"
                       "<li>支持配置主类和启动参数</li>"
                       "<li>支持两种启动模式</li>"
                       "<li>配置文件保存和加载</li>"
                       "<li>实时日志输出</li>"
                       "</ul>"
                       "<p>© 2024 JAR Packager Team</p>");
}

void JarPackagerWindow::closeEvent(QCloseEvent *event) {
    if (configChanged && !currentConfigPath.isEmpty()) {
        if (const int ret = QMessageBox::question(this, "保存配置", "配置已修改，是否保存？",
                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            ret == QMessageBox::Save) {
            savePackageConfig(currentConfigPath);
            saveSoftConfig(QDir::current().filePath(softConfigName));
            event->accept();
        } else if (ret == QMessageBox::Discard) {
            qInfo() << "用户选择不保存配置，直接退出";
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void JarPackagerWindow::updateStatus(const QString &message) const { ui->statusbar->showMessage(message); }

void JarPackagerWindow::loadPackageConfig(const QString &filePath) {
    qInfo() << QString("开始加载打包配置文件: %1").arg(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        const QString errorMsg = QString("无法打开打包配置文件: %1").arg(file.errorString());
        qWarning() << errorMsg;
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        const QString errorMsg = QString("解析打包配置文件失败: %1").arg(parseError.errorString());
        qWarning() << errorMsg;
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    PackageConfig config{};
    config.fromJson(doc.object());

    // 设置界面值
    ui->jarEdit->setText(config.jarPath);
    ui->outEdit->setText(config.outputPath);
    ui->jvmArgsEdit->setText(config.jvmArgs.join(";"));
    ui->progArgsEdit->setText(config.programArgs.join(";"));
    ui->javaPathEdit->setText(config.javaPath);
    ui->jarExtractPathEdit->setText(config.jarExtractPath);
    if (config.launchMode == static_cast<int>(JarCommon::LaunchMode::DirectJVM)) {
        ui->modeJvm->setChecked(true);
    } else {
        ui->modeJava->setChecked(true);
    }
    for (const auto &[verStr, verCode]: JarCommon::JAVA_VERSION_MAP) {
        if (verCode == config.javaVersion) {
            ui->javaVersionComboBox->setCurrentText(QString(verStr.c_str()));
            break;
        }
    }
    ui->mainClassEdit->setText(config.mainClass);
    ui->enablSplashCheckBox->setChecked(config.enableSplash);
    ui->splashShowProgressCheckBox->setChecked(config.splashShowProgress);
    ui->splashShowProgresstTextCheckBox->setChecked(config.splashShowProgressText);
    ui->launchTimeEdit->setText(QString::number(config.launchTime));
    ui->splashImageEdit->setText(config.splashImagePath);
    ui->splashNameEdit->setText(config.splashProgramName);
    ui->splashVersionEdit->setText(config.splashProgramVersion);
    ui->iconPathEdit->setText(config.iconPath);
    ui->showConsoleCheckBox->setChecked(config.showConsole);
    ui->requireAdminCheckBox->setChecked(config.requireAdmin);
    ui->externalExePathEdit->setText(config.externalExePath);

    currentConfigPath = filePath;
    configChanged = false;
    updateStatus(QString("已加载打包配置: %1").arg(QFileInfo(filePath).baseName()));
    qInfo() << "已打包加载配置, " << filePath;
}

void JarPackagerWindow::savePackageConfig(const QString &filePath) {
    PackageConfig config{};
    config.jarPath = ui->jarEdit->text().trimmed();
    config.outputPath = ui->outEdit->text().trimmed();
    config.jvmArgs = ui->jvmArgsEdit->text().split(";", Qt::SkipEmptyParts);
    config.programArgs = ui->progArgsEdit->text().split(";", Qt::SkipEmptyParts);
    // 清理参数列表中的空格
    for (QString &arg: config.jvmArgs) {
        arg = arg.trimmed();
    }
    for (QString &arg: config.programArgs) {
        arg = arg.trimmed();
    }
    config.javaPath = ui->javaPathEdit->text().trimmed();
    config.jarExtractPath = ui->jarExtractPathEdit->text().trimmed();
    config.launchMode = static_cast<int>(ui->modeJvm->isChecked()
                                             ? JarCommon::LaunchMode::DirectJVM
                                             : JarCommon::LaunchMode::JavaExe);
    if (const auto &javaVersion = ui->javaVersionComboBox->currentText(); !javaVersion.isEmpty()) {
        if (const std::string &ver = javaVersion.toStdString(); JarCommon::JAVA_VERSION_MAP.contains(ver)) {
            config.javaVersion = JarCommon::JAVA_VERSION_MAP.at(ver);
        }
    }
    config.mainClass = ui->mainClassEdit->text().trimmed();
    config.enableSplash = ui->enablSplashCheckBox->isChecked();
    config.splashShowProgress = ui->splashShowProgressCheckBox->isChecked();
    config.splashShowProgressText = ui->splashShowProgresstTextCheckBox->isChecked();
    config.launchTime = ui->launchTimeEdit->text().trimmed().toInt();
    config.splashShowProgress = ui->splashShowProgressCheckBox->isChecked();
    config.splashImagePath = ui->splashImageEdit->text().trimmed();
    config.splashProgramName = ui->splashNameEdit->text().trimmed();
    config.splashProgramVersion = ui->splashVersionEdit->text().trimmed();
    config.iconPath = ui->iconPathEdit->text().trimmed();
    config.showConsole = ui->showConsoleCheckBox->isChecked();
    config.requireAdmin = ui->requireAdminCheckBox->isChecked();
    config.externalExePath = ui->externalExePathEdit->text().trimmed();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString errorMsg = QString("无法创建打包配置文件: %1").arg(file.errorString());
        qWarning() << errorMsg;
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    const QJsonDocument doc(config.toJson());
    file.write(doc.toJson());
    file.close();

    currentConfigPath = filePath;
    configChanged = false;
    updateStatus(QString("已保存打包配置: %1").arg(QFileInfo(filePath).baseName()));
    qInfo() << "✓ 打包配置文件保存成功, " << filePath;
}

void JarPackagerWindow::loadSoftConfig(const QString &filePath) {
    qInfo() << QString("开始加载软件配置文件: %1").arg(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        const QString errorMsg = QString("无法打开软件配置文件: %1").arg(file.errorString());
        qWarning() << errorMsg;
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        const QString errorMsg = QString("解析软件配置文件失败: %1").arg(parseError.errorString());
        qWarning() << errorMsg;
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    SoftConfig config{};
    config.fromJson(doc.object());
    currentConfigPath = config.lastSoftConfigPath;
}

void JarPackagerWindow::saveSoftConfig(const QString &filePath) {
    SoftConfig config{};

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString errorMsg = QString("无法创建软件配置文件: %1").arg(file.errorString());
        qWarning() << errorMsg;
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    config.lastSoftConfigPath = currentConfigPath;

    const QJsonDocument doc(config.toJson());
    file.write(doc.toJson());
    file.close();

    updateStatus(QString("已保存软件配置: %1").arg(QFileInfo(filePath).baseName()));
    qInfo() << "✓ 软件配置文件保存成功, " << filePath;
}
