#ifndef TASKCONFIGDIALOG_H
#define TASKCONFIGDIALOG_H

#include <QDialog>
#include "SettingsParser.h"

class QLineEdit;
class QCheckBox;
class QComboBox;
class QTableWidget;
class QPushButton;

class TaskConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TaskConfigDialog(const QString &taskDir, const QString &taskName, QWidget *parent = nullptr);
    ~TaskConfigDialog() {}

private slots:
    void onSave();
    void onCancel();
    void onPrevTask();
    void onNextTask();

private:
    void loadConfig();
    void saveConfig();
    void setupUi();

    QString m_taskDir;
    QString m_taskName;
    TaskConfig m_config;

    QLineEdit *m_editInpFile;
    QCheckBox *m_chkStdIn;
    QLineEdit *m_editOutFile;
    QCheckBox *m_chkStdOut;
    QComboBox *m_cmbChecker;
    QCheckBox *m_chkActive;

    QTableWidget *m_table;
    QPushButton *m_btnPrev;
    QPushButton *m_btnNext;
    QPushButton *m_btnSave;
    QPushButton *m_btnCancel;
};

#endif // TASKCONFIGDIALOG_H
