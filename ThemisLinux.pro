QT       += core gui widgets xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ThemisLinux
TEMPLATE = app

SOURCES += main.cpp \
           MainWindow.cpp \
           SettingsParser.cpp \
           Judger.cpp \
           JudgeEngine.cpp \
           JudgeDialog.cpp \
           CompilerConfigDialog.cpp \
    TaskConfigDialog.cpp \
    DetailDialog.cpp \
           SecurityDialog.cpp \
           EnvSettingsDialog.cpp \
           Judger_win.cpp

HEADERS += MainWindow.h \
           SettingsParser.h \
           Judger.h \
           JudgeEngine.h \
           JudgeDialog.h \
           JudgeTypes.h \
           CompilerConfigDialog.h \
    TaskConfigDialog.h \
    DetailDialog.h \
           SecurityDialog.h \
           EnvSettingsDialog.h

LIBS += -lz

RESOURCES += resources.qrc
