#ifndef SETTINGSPARSER_H
#define SETTINGSPARSER_H

#include <QString>
#include <QList>

struct TestCaseConfig {
    QString name;
    double mark;
    double timeLimit;
    double memoryLimit;
};

struct TaskConfig {
    QString name;
    QString inputFile;
    QString outputFile;
    bool useStdIn;
    bool useStdOut;
    QString evaluatorName;
    double mark;
    double timeLimit;
    double memoryLimit;
    QList<TestCaseConfig> testCases;
};

class SettingsParser
{
public:
    static TaskConfig parse(const QString &filePath);
    static bool save(const QString &filePath, const TaskConfig &config);
};

#endif // SETTINGSPARSER_H
