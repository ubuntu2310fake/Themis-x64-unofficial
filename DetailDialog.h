#pragma once
#include <QDialog>
#include <QString>

class QTreeWidget;
class QCheckBox;

class DetailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DetailDialog(const QString &contestantName,
                          const QString &logPath,
                          const QString &filterTask = QString(),
                          QWidget *parent = nullptr);

private slots:
    void updateView();

private:
    void loadLog();

    QString m_contestantName;
    QString m_logPath;
    QString m_filterTask;

    QTreeWidget *m_tree;
    QCheckBox *m_chkAC;
    QCheckBox *m_chkWA;
    QCheckBox *m_chkPartial;
};
