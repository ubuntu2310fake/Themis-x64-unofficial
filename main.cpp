#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Themis Linux");
    a.setApplicationVersion("1.0");
    
    MainWindow w;
    w.resize(1024, 768);
    w.show();
    
    return a.exec();
}
