#include "jarpackager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <algorithm>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <cstdio>
#endif

namespace {
    void setupApplicationInfo() {
        QCoreApplication::setApplicationName("JAR Packager");
        QCoreApplication::setApplicationVersion("1.0.0");
        QCoreApplication::setOrganizationName("XiaoJiawei");
        QCoreApplication::setOrganizationDomain("xiaojiawei.club");
    }

    bool isHelpArgument(const QString &arg) {
        return arg == "-h" || arg == "--help" || arg == "/?";
    }

    QString commandLineConfigPath(const int argc, char *argv[]) {
        for (int i = 1; i < argc; ++i) {
            const QString arg = QString::fromLocal8Bit(argv[i]);
            if (arg == "-c" || arg == "--config" || arg == "-config" || arg == "/config") {
                return i + 1 < argc ? QString::fromLocal8Bit(argv[i + 1]) : QString();
            }
            if (arg.startsWith("--config=")) {
                return arg.mid(QString("--config=").size());
            }
            if (arg.startsWith("-c=")) {
                return arg.mid(QString("-c=").size());
            }
        }

        if (argc == 2) {
            return QString::fromLocal8Bit(argv[1]);
        }
        return {};
    }

    bool shouldRunCommandLine(const int argc, char *argv[]) {
        return argc > 1;
    }

#ifdef Q_OS_WIN
    void attachParentConsole() {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            return;
        }

        FILE *stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
    }
#else
    void attachParentConsole() {
    }
#endif

    void commandLineMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
        QTextStream stream(type == QtInfoMsg || type == QtDebugMsg ? stdout : stderr);
        stream << msg << Qt::endl;
    }

    int runCommandLine(int argc, char *argv[]) {
        attachParentConsole();
        QCoreApplication app(argc, argv);
        setupApplicationInfo();
        qInstallMessageHandler(commandLineMessageHandler);

        const QStringList args = QCoreApplication::arguments();
        const bool showHelp = std::any_of(args.begin(), args.end(), isHelpArgument);
        if (showHelp) {
            QTextStream(stdout) << "用法:\n"
                    << "  JarPackager.exe --config <配置文件.json>\n"
                    << "  JarPackager.exe <配置文件.json>\n";
            return 0;
        }

        const QString configPath = commandLineConfigPath(argc, argv);
        if (configPath.isEmpty()) {
            QTextStream(stderr) << "缺少配置文件路径，请使用 --config <配置文件>" << Qt::endl;
            return 2;
        }

        const auto res = Packager::packageFromConfigFile(configPath, QCoreApplication::applicationFilePath());
        if (!res) {
            QTextStream(stderr) << "打包失败: " << res.error() << Qt::endl;
            return 1;
        }

        QTextStream(stdout) << "打包完成: " << res->outputPath << Qt::endl;
        if (!res->zipError.isEmpty()) {
            QTextStream(stderr) << "压缩包创建失败: " << res->zipError << Qt::endl;
            return 3;
        }
        return 0;
    }
}

int main(int argc, char *argv[]) {
    if (shouldRunCommandLine(argc, argv)) {
        const auto res = runCommandLine(argc, argv);
        exit(res);
    }

    QApplication app(argc, argv);
    setupApplicationInfo();

    // 设置应用程序图标（如果有的话）
    // app.setWindowIcon(QIcon(":/icons/app.ico"));

    // 设置样式
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QApplication::setStyle(QStyleFactory::create("windowsvista"));

    JarPackagerWindow window;
    window.show();

    return QApplication::exec();
}
