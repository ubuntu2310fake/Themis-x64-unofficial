#pragma once
#include "JudgeEngine.h"
#include <QDialog>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>

class JudgeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit JudgeDialog(JudgeEngine *engine, const QString &mode = "classic", QWidget *parent = nullptr);
    ~JudgeDialog();

signals:
    // Emitted when judging finishes so MainWindow can refresh the table
    void judgeFinished(const QMap<QString, QMap<QString, double>> &scores);

private slots:
    void onUpdate(const JudgeUpdate &u);
    void onStatusChanged(const QString &contestant, const QString &task, const QString &status);
    void onProgress(int done, int total);
    void onLog(const QString &msg);
    void onFinished();
    void updateTimerDisplay();

private:
    void buildUI();
    void buildClassicUI();
    void buildDetailedUI();

    JudgeEngine   *m_engine;
    QString        m_mode; // "classic" or "detailed"

    // Widgets
    QProgressBar  *m_progress = nullptr;
    QLabel        *m_lblName = nullptr;
    QLabel        *m_lblStatus = nullptr;
    QLabel        *m_lblTime = nullptr;
    QTextEdit     *m_log = nullptr;

    // Timer
    QTimer        *m_timer = nullptr;
    QElapsedTimer  m_elapsed;

    // [contestant][task] = total score
    QMap<QString, QMap<QString, double>> m_scores;
};
