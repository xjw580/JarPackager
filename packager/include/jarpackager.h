#pragma once

#include <QRadioButton>
#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMainWindow>
#include <expected>

#include "jarcommon.h"


QT_BEGIN_NAMESPACE

namespace Ui {
    class JarPackagerWindow;
}

QT_END_NAMESPACE


class PackageConfig {
public:
    QString jarPath{};
    QString outputPath{};
    QStringList jvmArgs{};
    QStringList programArgs{};
    QString javaPath{};
    QString jarExtractPath{};
    int launchMode = 0;
    qint64 javaVersion = 0;
    QString mainClass{};
    bool enableSplash = false;
    QString splashImagePath{};
    bool splashShowProgress = false;
    bool splashShowProgressText = false;
    int launchTime = 0; // 单位：ms
    QString splashProgramName{};
    QString splashProgramVersion{};
    QString iconPath{};
    bool showConsole = false;
    bool requireAdmin = false;
    QString externalExePath{};

    [[nodiscard]] QJsonObject toJson() const;

    void fromJson(const QJsonObject &obj);
};

class SoftConfig {
public:
    QString lastSoftConfigPath{};

    [[nodiscard]] QJsonObject toJson() const;

    void fromJson(const QJsonObject &obj);
};

class Packager {
    Packager() = delete;

    ~Packager() = delete;

public:
    struct Config {
        QByteArray exeData;
        QString jarPath;
        QString splashImagePath;
        bool splashShowProgress;
        bool splashShowProgressText;
        int launchTime; // 单位：ms
        unsigned int javaVersion;
        QString outputPath;
        QString mainClass;
        QStringList jvmArgs;
        QStringList programArgs;
        QString javaPath;
        QString jarExtractPath;
        QString splashProgramName;
        QString splashProgramVersion;
        JarCommon::LaunchMode launchMode;
        QString iconPath;
        bool showConsole;
        bool requireAdmin;

        Config(const QByteArray &exeData_, const QString &jarPath_, const QString &splashImagePath_,
               const bool splashShowProgress_, const bool splashShowProgressText_, int launchTime_,
               unsigned int javaVersion_, const QString &outputPath_, const QString &mainClass_,
               const QStringList &jvmArgs_, const QStringList &programArgs_, const QString &javaPath_,
               const QString &jarExtractPath_, const QString &splashProgramName_, const QString &splashProgramVersion_,
               const JarCommon::LaunchMode launchMode_, const QString &iconPath_, const bool showConsole_,
               const bool requireAdmin_) : exeData(exeData_), jarPath(jarPath_), splashImagePath(splashImagePath_),
                                           splashShowProgress(splashShowProgress_),
                                           splashShowProgressText(splashShowProgressText_),
                                           launchTime(launchTime_), javaVersion(javaVersion_), outputPath(outputPath_),
                                           mainClass(mainClass_),
                                           jvmArgs(jvmArgs_), programArgs(programArgs_), javaPath(javaPath_),
                                           jarExtractPath(jarExtractPath_),
                                           splashProgramName(splashProgramName_),
                                           splashProgramVersion(splashProgramVersion_), launchMode(launchMode_),
                                           iconPath(iconPath_), showConsole(showConsole_), requireAdmin(requireAdmin_) {
        }
    };

    static std::expected<bool, QString> packageJar(const Config &config);

    static std::expected<bool, QString> extractJarInfo(const QString &jarPath, PackageConfig &jarInfo);

    static std::expected<bool, QString> modifyExe(const QString &exePath, const QString &iconPath, bool showConsole,
                                                  bool requireAdmin);
};

class JarPackagerWindow final : public QMainWindow {
    Q_OBJECT

public:
    inline static QString softConfigName = "soft.config";

    explicit JarPackagerWindow(QWidget *parent = nullptr);

    ~JarPackagerWindow() override;

    // 静态方法用于Qt消息处理
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    static JarPackagerWindow *instance; // 静态实例指针
    static bool openAndSelectFile(const QString &path);

private slots:
    void on_enablSplashCheckBox_stateChanged(int state);

    void on_jarBtn_clicked();

    void on_outBtn_clicked();

    void on_javaPathBtn_clicked();

    void on_loadConfigBtn_clicked();

    void on_saveConfigBtn_clicked();

    void on_packageBtn_clicked();

    void on_loadExeBtn_clicked();

    void on_modifyExeBtn_clicked();

    void on_attachExeAction_triggered();

    void on_splashImageBtn_clicked();

    void on_iconBtn_clicked();

    void on_modeButtonGroup_idToggled(int id, bool checked);

    // 菜单动作槽函数
    void on_actionLoadConfig_triggered();

    void on_actionSaveConfig_triggered();

    void on_actionExit_triggered();

    void on_actionAbout_triggered();

    // 日志输出槽函数
    void appendLogMessage(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

private:
    void updateStatus(const QString &message) const;

    void loadPackageConfig(const QString &filePath);

    void savePackageConfig(const QString &filePath);

    void loadSoftConfig(const QString &filePath);

    void saveSoftConfig(const QString &filePath);

    void setupLogging(); // 设置日志系统

private:
    Ui::JarPackagerWindow *ui;
    QString currentConfigPath;
    bool configChanged = false;
    QMap<int, QRadioButton *> modeMap;
};
