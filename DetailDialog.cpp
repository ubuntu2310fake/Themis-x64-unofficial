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
#include <QFont>
#include <QColor>

DetailDialog::DetailDialog(const QString &contestantName, const QString &logPath, const QString &filterTask, QWidget *parent)
    : QDialog(parent), m_contestantName(contestantName), m_logPath(logPath), m_filterTask(filterTask)
{
    if (!m_filterTask.isEmpty()) {
        setWindowTitle(QString("Chi tiết bài làm: %1 - %2").arg(m_filterTask, m_contestantName));
    } else {
        setWindowTitle("Chi tiết chấm - " + m_contestantName);
    }
    resize(640, 500);

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(10, 10, 10, 10);
    mainLay->setSpacing(8);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(false);
    m_tree->setStyleSheet(
        "QTreeWidget { background-color: #ffffff; border: 1px solid #c0c0c0; font-size: 13px; }"
        "QTreeWidget::item { padding: 2px 0px; }"
    );
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
    chkLay->addStretch();
    mainLay->addLayout(chkLay);

    QHBoxLayout *btnLay = new QHBoxLayout();
    QPushButton *btnCopy = new QPushButton("Copy");
    btnCopy->setMinimumWidth(80);
    QPushButton *btnClose = new QPushButton("Đóng");
    btnClose->setMinimumWidth(80);

    btnLay->addWidget(btnCopy);
    btnLay->addStretch();
    btnLay->addWidget(btnClose);
    mainLay->addLayout(btnLay);

    connect(btnClose, &QPushButton::clicked, this, &DetailDialog::accept);
    connect(m_chkAC, &QCheckBox::toggled, this, &DetailDialog::updateView);
    connect(m_chkWA, &QCheckBox::toggled, this, &DetailDialog::updateView);
    connect(m_chkPartial, &QCheckBox::toggled, this, &DetailDialog::updateView);

    connect(btnCopy, &QPushButton::clicked, this, [this]() {
        QString text;
        auto dumpTree = [&](auto self, QTreeWidgetItem *item, int depth) -> void {
            if (!item || item->isHidden()) return;
            text += QString("   ").repeated(depth) + item->text(0) + "\n";
            for (int i = 0; i < item->childCount(); ++i) {
                self(self, item->child(i), depth + 1);
            }
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            dumpTree(dumpTree, m_tree->topLevelItem(i), 0);
        }
        QApplication::clipboard()->setText(text);
    });

    loadLog();
}

void DetailDialog::loadLog()
{
    m_tree->clear();
    QTreeWidgetItem *root = new QTreeWidgetItem(m_tree, { "■ " + m_contestantName + ":" });
    root->setExpanded(true);
    QFont rf = root->font(0);
    rf.setBold(true);
    root->setFont(0, rf);

    QFile file(m_logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        new QTreeWidgetItem(root, { "Không tìm thấy dữ liệu chấm." });
        return;
    }

    QTextStream in(&file);
    struct TaskBlock {
        QString taskName;
        QString sourceFile;
        QString scoreStr;
        bool isCE = false;
        QString ceLog;
        bool isSK = false;
        struct TestEntry {
            QString name;
            QString verdict;
            QString timeSec;
            QString memKb;
            QString score;
            QString msg;
        };
        QList<TestEntry> tests;
    };

    QList<TaskBlock> blocks;
    TaskBlock curBlock;
    bool inBlock = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
            if (inBlock) blocks.append(curBlock);
            curBlock = TaskBlock();
            curBlock.taskName = trimmed.mid(1, trimmed.length() - 2);
            inBlock = true;
        } else if (inBlock) {
            if (trimmed.startsWith("Source=")) {
                curBlock.sourceFile = trimmed.mid(7);
            } else if (trimmed.startsWith("Score=")) {
                curBlock.scoreStr = trimmed.mid(6);
            } else if (trimmed == "CE") {
                curBlock.isCE = true;
                while (!in.atEnd()) {
                    QString l = in.readLine();
                    if (l.trimmed().startsWith("[")) {
                        blocks.append(curBlock);
                        curBlock = TaskBlock();
                        curBlock.taskName = l.trimmed().mid(1, l.trimmed().length() - 2);
                        break;
                    }
                    curBlock.ceLog += l + "\n";
                }
            } else if (trimmed == "SK") {
                curBlock.isSK = true;
            } else if (trimmed == "OK") {
                // marker line, ignore
            } else if (trimmed.contains("|")) {
                QStringList parts = trimmed.split("|");
                if (parts.size() >= 4) {
                    TaskBlock::TestEntry te;
                    te.name = parts[0];
                    te.verdict = parts[1];
                    double tVal = parts[2].toDouble();
                    if (tVal > 50.0) tVal /= 1000.0;
                    te.timeSec = QString::number(tVal, 'f', 6);
                    te.memKb = parts[3];
                    if (parts.size() >= 6) {
                        te.score = parts[4];
                        te.msg = parts[5];
                    } else if (parts.size() == 5) {
                        te.msg = parts[4];
                        te.score = (te.verdict == "AC") ? "1.00" : "0.00";
                    } else {
                        te.score = (te.verdict == "AC") ? "1.00" : "0.00";
                    }
                    curBlock.tests.append(te);
                }
            }
        }
    }
    if (inBlock) blocks.append(curBlock);
    file.close();

    bool foundAny = false;
    for (const TaskBlock &tb : blocks) {
        if (!m_filterTask.isEmpty() && tb.taskName.compare(m_filterTask, Qt::CaseInsensitive) != 0) {
            continue;
        }
        foundAny = true;

        QString taskHeader = "◆ " + tb.taskName;
        if (!tb.scoreStr.isEmpty()) taskHeader += ": " + tb.scoreStr;
        else if (tb.isCE) taskHeader += ": Dịch lỗi";
        else if (tb.isSK) taskHeader += ": Không có bài";

        QTreeWidgetItem *taskItem = new QTreeWidgetItem(root, { taskHeader });
        QFont tf = taskItem->font(0);
        tf.setBold(true);
        taskItem->setFont(0, tf);
        taskItem->setExpanded(true);

        if (!tb.sourceFile.isEmpty()) {
            new QTreeWidgetItem(taskItem, { tb.sourceFile });
        }

        if (tb.isCE) {
            auto *ceItem = new QTreeWidgetItem(taskItem, { "Dịch lỗi: " + tb.ceLog.trimmed() });
            ceItem->setForeground(0, QColor("#cc0000"));
        } else if (tb.isSK) {
            auto *skItem = new QTreeWidgetItem(taskItem, { "Chưa chấm / Không có bài" });
            skItem->setForeground(0, QColor("#888888"));
        } else {
            auto *okItem = new QTreeWidgetItem(taskItem, { "Dịch thành công" });
            okItem->setForeground(0, QColor("#006600"));

            for (const TaskBlock::TestEntry &te : tb.tests) {
                QString testHeader = QString("● %1: %2").arg(te.name, te.score);
                QTreeWidgetItem *testItem = new QTreeWidgetItem(taskItem, { testHeader });
                testItem->setExpanded(true);

                double sc = te.score.toDouble();
                if (te.verdict == "AC" || sc >= 1.0) {
                    testItem->setForeground(0, QColor("#008000"));
                    testItem->setData(0, Qt::UserRole, "AC");
                } else if (sc > 0.0) {
                    testItem->setForeground(0, QColor("#cc6600"));
                    testItem->setData(0, Qt::UserRole, "PARTIAL");
                } else {
                    testItem->setForeground(0, QColor("#cc0000"));
                    testItem->setData(0, Qt::UserRole, "WA");
                }

                new QTreeWidgetItem(testItem, { QString("   Thời gian ≈ %1 giây").arg(te.timeSec) });
                new QTreeWidgetItem(testItem, { "   Kiểm tra kết quả:" });
                QString msg = te.msg.isEmpty() ? (te.verdict == "AC" ? "Kết quả đúng!" : "Kết quả sai!") : te.msg;
                new QTreeWidgetItem(testItem, { "   " + msg });
            }
        }
    }

    if (!foundAny) {
        if (!m_filterTask.isEmpty()) {
            new QTreeWidgetItem(root, { QString("Chưa có dữ liệu chấm cho bài %1.").arg(m_filterTask) });
        } else {
            new QTreeWidgetItem(root, { "Không tìm thấy dữ liệu chấm." });
        }
    }

    updateView();
}

void DetailDialog::updateView()
{
    bool showAC = m_chkAC->isChecked();
    bool showWA = m_chkWA->isChecked();
    bool showPartial = m_chkPartial->isChecked();

    if (m_tree->topLevelItemCount() == 0) return;
    QTreeWidgetItem *root = m_tree->topLevelItem(0);

    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem *taskItem = root->child(i);
        for (int j = 0; j < taskItem->childCount(); ++j) {
            QTreeWidgetItem *child = taskItem->child(j);
            QString role = child->data(0, Qt::UserRole).toString();
            if (role == "AC") {
                child->setHidden(!showAC);
            } else if (role == "WA") {
                child->setHidden(!showWA);
            } else if (role == "PARTIAL") {
                child->setHidden(!showPartial);
            }
        }
    }
}
