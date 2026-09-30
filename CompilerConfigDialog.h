#ifndef COMPILERCONFIGDIALOG_H
#define COMPILERCONFIGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QList>

struct CompilerEntry {
    QString ext;      // e.g. ".cpp"
    QString command;  // e.g. "/usr/bin/g++ -O2 ..."
};

class CompilerConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CompilerConfigDialog(QWidget *parent = nullptr);
    ~CompilerConfigDialog() {}

    static QList<CompilerEntry> defaultEntries();

    QList<CompilerEntry> entries() const;
    void setEntries(const QList<CompilerEntry> &entries);

private slots:
    void onInsertRow();
    void onDeleteRow();
    void onMoveUp();
    void onMoveDown();
    void onBrowseExe();
    void onCurrentCellChanged(int currentRow, int currentCol, int, int);
    void accept() override;

private:
    void buildUI();
    void refreshTable();
    bool isCompilerRow(int row) const;
    bool isPythonRow(int row) const;
    QString browseLabelFor(int row) const;

    QTableWidget *m_table;
    QWidget      *m_browseWidget;
    QPushButton  *m_btnBrowseExe;
    QLabel       *m_labelBrowseHint;

    QList<CompilerEntry> m_entries;
};

#endif // COMPILERCONFIGDIALOG_H
