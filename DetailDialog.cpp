#include "DetailDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QCheckBox>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>

DetailDialog::DetailDialog(const QString &contestantName, const QString &logPath, QWidget *parent)
    : QDialog(parent), m_contestantName(contestantName), m_logPath(logPath)
{
    setWindowTitle("Chi tiết chấm");
    resize(600, 450);

    QVBoxLayout *mainLay = new QVBoxLayout(this);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    mainLay->addWidget(m_tree, 1);

    QHBoxLayout *chkLay = new QHBoxLayout();
    m_chkAC = new QCheckBox("Hiện test đúng");
    m_chkAC->setChecked(true);
    m_chkWA = new QCheckBox("Hiện test sai");
    m_chkWA->setChecked(true);
    m_chkPartial = new QCheckBox("Hiện test đúng một phần");
    m_chkPartial->setChecked(true);
    
    chkLay->addWidget(m_chkAC);
    chkLay->addWidget(m_chkWA);
    chkLay->addWidget(m_chkPartial);
    mainLay->addLayout(chkLay);

    QHBoxLayout *btnLay = new QHBoxLayout();
    QPushButton *btnCopy = new QPushButton("Copy");
    btnCopy->setIcon(QIcon::fromTheme("edit-copy"));
    QPushButton *btnClose = new QPushButton("Đóng");
    btnClose->setIcon(QIcon::fromTheme("window-close"));
    
    btnLay->addWidget(btnCopy);
    btnLay->addStretch();
    btnLay->addWidget(btnClose);
    mainLay->addLayout(btnLay);

    connect(btnClose, &QPushButton::clicked, this, &DetailDialog::accept);
    connect(m_chkAC, &QCheckBox::toggled, this, &DetailDialog::updateView);
    connect(m_chkWA, &QCheckBox::toggled, this, &DetailDialog::updateView);
    connect(m_chkPartial, &QCheckBox::toggled, this, &DetailDialog::updateView);

    connect(btnCopy, &QPushButton::clicked, this, [this]() {
        QFile file(m_logPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QApplication::clipboard()->setText(file.readAll());
            file.close();
        }
    });

    loadLog();
}

void DetailDialog::loadLog()
{
    m_tree->clear();
    QTreeWidgetItem *root = new QTreeWidgetItem(m_tree, { "■ " + m_contestantName + ":" });
    root->setExpanded(true);
    
    QFile file(m_logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        new QTreeWidgetItem(root, { "Không tìm thấy dữ liệu chấm." });
        return;
    }

    QTextStream in(&file);
    QTreeWidgetItem *curTask = nullptr;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith("[") && line.endsWith("]")) {
            QString tname = line.mid(1, line.length() - 2);
            curTask = new QTreeWidgetItem(root, { "● " + tname });
            curTask->setExpanded(true);
        } else if (curTask) {
            if (line == "CE") {
                QString ceLog;
                while (!in.atEnd()) {
                    QString l = in.readLine();
                    if (l.startsWith("[")) {
                        // next task
                        QString tname = l.mid(1, l.length() - 2);
                        curTask = new QTreeWidgetItem(root, { "● " + tname });
                        curTask->setExpanded(true);
                        break;
                    }
                    ceLog += l + "\n";
                }
                new QTreeWidgetItem(curTask, { "Dịch lỗi: " + ceLog.trimmed() });
                curTask->setText(0, curTask->text(0) + ": Dịch lỗi");
            } else if (line == "SK") {
                new QTreeWidgetItem(curTask, { "Chưa chấm" });
                curTask->setText(0, curTask->text(0) + ": Chưa chấm");
            } else {
                // Parse: testName|verdict|time|mem|msg
                QStringList parts = line.split("|");
                if (parts.size() >= 4) {
                    QString testName = parts[0];
                    QString verdict = parts[1];
                    QString time = parts[2] + "ms";
                    QString mem = parts[3] + "KB";
                    QString msg = parts.size() > 4 ? parts[4] : "";
                    
                    QTreeWidgetItem *item = new QTreeWidgetItem(curTask, { QString("%1: %2 %3 %4 %5").arg(testName).arg(verdict).arg(time).arg(mem).arg(msg) });
                    item->setData(0, Qt::UserRole, verdict);
                }
            }
        }
    }
    
    updateView();
}

void DetailDialog::updateView()
{
    bool showAC = m_chkAC->isChecked();
    bool showWA = m_chkWA->isChecked();
    
    if (m_tree->topLevelItemCount() == 0) return;
    QTreeWidgetItem *root = m_tree->topLevelItem(0);
    
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem *taskItem = root->child(i);
        for (int j = 0; j < taskItem->childCount(); ++j) {
            QTreeWidgetItem *testItem = taskItem->child(j);
            QString v = testItem->data(0, Qt::UserRole).toString();
            if (v == "AC" && !showAC) testItem->setHidden(true);
            else if (v != "AC" && v != "" && !showWA) testItem->setHidden(true);
            else testItem->setHidden(false);
        }
    }
}
