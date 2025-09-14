#include <QLibraryInfo>
#include <QTranslator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>
#include "jarpackager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    QApplication::setApplicationName("JAR Packager");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("JAR Packager Team");
    QApplication::setOrganizationDomain("jarpackager.com");

    // 设置应用程序图标（如果有的话）
    // app.setWindowIcon(QIcon(":/icons/app.ico"));

    // 设置样式
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QApplication::setStyle(QStyleFactory::create("windowsvista"));
    
    JarPackagerWindow window;
    window.show();

    return QApplication::exec();
}