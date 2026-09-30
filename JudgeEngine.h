#pragma once
#include "JudgeTypes.h"
#include "SettingsParser.h"
#include "CompilerConfigDialog.h"
#include <QObject>
#include <QThread>
#include <QList>

// ─── Signal struct for scoreboard updates ─────────────────────────────────────
struct JudgeUpdate {
    QString  contestantName;
    QString  taskName;
    int      testIndex;     // -1 = task-level update (CE, SK)
    TestResult testResult;
    double   taskScore;     // running total for this task
};

// ─── Worker thread for one contestant ─────────────────────────────────────────
class JudgeWorker : public QThread
{
    Q_OBJECT
public:
    struct Config {
        QString  contestantDir;     // full path to contestant folder
        QString  contestantName;
        QString  tasksDir;          // full path to Tasks folder
        QStringList taskNames;
        QList<CompilerEntry> compilers;
        QString  workDir;           // temp working directory
    };

    explicit JudgeWorker(const Config &cfg, QObject *parent = nullptr);

signals:
    void updateReady(const JudgeUpdate &update);
    void contestantDone(const QString &name, const QList<TaskResult> &results);
    void logMessage(const QString &msg);

protected:
    void run() override;

private:
    TaskResult judgeTask(const QString &taskName);
    const CompilerEntry* findCompiler(const QString &ext) const;
    QString findSourceFile(const QString &taskName, QString &ext) const;

    Config m_cfg;
};

// ─── High-level Orchestrator ──────────────────────────────────────────────────
class JudgeEngine : public QObject
{
    Q_OBJECT
public:
    struct ContestConfig {
        QString  tasksDir;
        QString  contestantsDir;
        QStringList taskNames;
        QStringList contestantNames;
        QList<CompilerEntry> compilers;
    };

    explicit JudgeEngine(const ContestConfig &cfg, QObject *parent = nullptr);
    ~JudgeEngine();

    void start();
    void stop();
    bool isRunning() const;

signals:
    void updateReady(const JudgeUpdate &update);
    void logMessage(const QString &msg);
    void progressChanged(int done, int total);
    void finished();

private slots:
    void onWorkerDone(const QString &name, const QList<TaskResult> &results);

private:
    ContestConfig m_cfg;
    QList<JudgeWorker*> m_workers;
    QString m_tempDir;
    int m_doneCount = 0;
    int m_total     = 0;
    bool m_stopped  = false;
};
