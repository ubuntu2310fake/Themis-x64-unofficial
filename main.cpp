#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Themis Linux");
    a.setApplicationVersion("1.0");

#ifdef Q_OS_WIN
    // Ensure bundled toolchain dirs are in process PATH for child processes and DLL loading
    QString appDir = QCoreApplication::applicationDirPath();
    QString curPath = qEnvironmentVariable("PATH");
    QStringList extraDirs = {
        appDir,
        appDir + "/GCC/bin",
        appDir + "/PYTHON",
        appDir + "/FPC/bin/i386-win32",
        appDir + "/JDK/bin"
    };
    for (const QString &d : extraDirs) {
        if (QFile::exists(d)) {
            curPath = QDir::toNativeSeparators(d) + ";" + curPath;
        }
    }
    qputenv("PATH", curPath.toLocal8Bit());
#endif

    MainWindow w;
    w.resize(1024, 768);
    w.show();
    
    return a.exec();
}
