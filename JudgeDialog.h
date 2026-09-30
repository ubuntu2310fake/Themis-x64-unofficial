#pragma once
#include "JudgeEngine.h"
#include <QDialog>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QMap>

class JudgeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit JudgeDialog(JudgeEngine *engine, QWidget *parent = nullptr);
    ~JudgeDialog();

signals:
    // Emitted when judging finishes so MainWindow can refresh the table
    void judgeFinished(const QMap<QString, QMap<QString, double>> &scores);

private slots:
    void onUpdate(const JudgeUpdate &u);
    void onProgress(int done, int total);
    void onLog(const QString &msg);
    void onFinished();

private:
    void buildUI();

    JudgeEngine   *m_engine;
    QProgressBar  *m_progress;
    QLabel        *m_lblStatus;
    QTextEdit     *m_log;

    // [contestant][task] = total score
    QMap<QString, QMap<QString, double>> m_scores;
};
