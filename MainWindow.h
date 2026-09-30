#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTableWidget>
#include <QAction>
#include <QLabel>
#include <QStringList>
#include "CompilerConfigDialog.h"
#include "JudgeEngine.h"
#include <QSet>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // ── Kỳ thi ──
    void onNewContest();
    void onOpenContest();
    void onSaveContest();
    void onSaveContestAs();
    void onEnvSettings();

    // ── Bài thi ──
    void onLoadTasks();
    void onRefreshTasks();
    void onSelectAllTasks();
    void onDeselectAllTasks();
    void onToggleTasks();

    // ── Thí sinh ──
    void onLoadContestants();
    void onRefreshContestants();
    void onSelectAllContestants();
    void onDeselectAllContestants();
    void onToggleContestants();

    // ── Chấm bài ──
    void onJudge();
    void onOnlineJudge();
    void onExportExcel();
    void onCompilerConfig();
    void onSecurity();
    void onJudgeUpdate(const JudgeUpdate &update);

    // ── Hướng dẫn ──
    void onHelp();
    void onAbout();

    // ── Internal ──
    void onColumnRightClick(const QPoint &pos);
    void onRowDoubleClick(int row, int col);
    void onHeaderDoubleClick(int col);

private:
    void buildMenu();
    void buildToolbar();
    void buildCentralWidget();
    void buildStatusBar();

    void updateActionStates();
    void rebuildTableColumns();
    void addContestantRow(const QString &name);

    // Data
    QStringList m_tasks;
    QSet<QString> m_selectedTasks;
    QStringList m_contestants;
    QString     m_tasksDir;
    QString     m_contestantsDir;
    QList<CompilerEntry> m_compilerEntries;

    // UI
    QTabWidget   *m_tabs;
    QTableWidget *m_table;
    QLabel       *m_statusLabel;

    // Actions needing enable/disable
    QAction *m_actRefreshTasks;
    QAction *m_actSelectAllTasks, *m_actDeselectAllTasks, *m_actToggleTasks;
    QAction *m_actRefreshContestants;
    QAction *m_actSelectAllContestants, *m_actDeselectAllContestants, *m_actToggleContestants;
    QAction *m_actJudge, *m_actOnlineJudge, *m_actExportExcel;
    QAction *m_actSave, *m_actSaveAs;
    QToolBar *m_toolbar;
};

#endif // MAINWINDOW_H
