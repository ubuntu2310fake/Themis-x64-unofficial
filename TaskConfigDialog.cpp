#include "TaskConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

TaskConfigDialog::TaskConfigDialog(const QString &taskDir, const QString &taskName, QWidget *parent)
    : QDialog(parent), m_taskDir(taskDir), m_taskName(taskName)
{
    setWindowTitle("Cấu hình bài thi: " + taskName);
    resize(700, 450);
    setupUi();
    loadConfig();
}

void TaskConfigDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *topLayout = new QHBoxLayout();

    // ─── Left Panel (General Config) ───
    QGroupBox *grpLeft = new QGroupBox("Cấu hình bài thi");
    QVBoxLayout *layLeft = new QVBoxLayout(grpLeft);

    layLeft->addWidget(new QLabel("Tên (các) file dữ liệu:"));
    QHBoxLayout *layInp = new QHBoxLayout();
    m_editInpFile = new QLineEdit();
    layInp->addWidget(m_editInpFile);
    layLeft->addLayout(layInp);

    m_chkStdIn = new QCheckBox("Dùng luồng vào chuẩn");
    layLeft->addWidget(m_chkStdIn);
    layLeft->addSpacing(10);

    layLeft->addWidget(new QLabel("Tên (các) file kết quả:"));
    QHBoxLayout *layOut = new QHBoxLayout();
    m_editOutFile = new QLineEdit();
    layOut->addWidget(m_editOutFile);
    layLeft->addLayout(layOut);

    m_chkStdOut = new QCheckBox("Dùng luồng ra chuẩn");
    layLeft->addWidget(m_chkStdOut);
    layLeft->addSpacing(10);

    layLeft->addWidget(new QLabel("Trình chấm:"));
    m_cmbChecker = new QComboBox();
    m_cmbChecker->addItems({
        "C1LinesWordsIgnoreCase.dll",
        "C2LinesWordsCase.dll",
        "C3WordsIgnoreCase.dll",
        "C4WordsCase.dll",
        "C5Binary.dll",
        "C7External.dll"
    });
    layLeft->addWidget(m_cmbChecker);

    m_chkActive = new QCheckBox("Có chấm bài này");
    m_chkActive->setChecked(true);
    layLeft->addWidget(m_chkActive);

    layLeft->addStretch();
    topLayout->addWidget(grpLeft, 1);

    // ─── Right Panel (Test Cases) ───
    QGroupBox *grpRight = new QGroupBox("Bộ test");
    QVBoxLayout *layRight = new QVBoxLayout(grpRight);

    m_table = new QTableWidget();
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Điểm\ncủa test", "Giới hạn\nthời gian (giây)", "Giới hạn\nbộ nhớ (MiB)"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setMinimumWidth(80);
    layRight->addWidget(m_table);

    topLayout->addWidget(grpRight, 2);
    mainLayout->addLayout(topLayout);

    // ─── Bottom Buttons ───
    QHBoxLayout *layBottom = new QHBoxLayout();
    
    QPushButton *btnHelp = new QPushButton("Hướng dẫn");
    btnHelp->setIcon(QIcon::fromTheme("help-faq"));
    
    m_btnPrev = new QPushButton("<");
    m_btnSave = new QPushButton("Ghi nhận");
    m_btnSave->setIcon(QIcon::fromTheme("document-save"));
    m_btnSave->setStyleSheet("font-weight: bold; background-color: #f1c40f;");
    m_btnNext = new QPushButton(">");
    
    m_btnCancel = new QPushButton("Hủy bỏ");
    m_btnCancel->setIcon(QIcon::fromTheme("window-close"));
    
    layBottom->addWidget(btnHelp);
    layBottom->addStretch();
    layBottom->addWidget(m_btnPrev);
    layBottom->addWidget(m_btnSave);
    layBottom->addWidget(m_btnNext);
    layBottom->addStretch();
    layBottom->addWidget(m_btnCancel);
    
    mainLayout->addLayout(layBottom);

    connect(m_btnSave, &QPushButton::clicked, this, &TaskConfigDialog::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &TaskConfigDialog::reject);
    // Prev/Next will be implemented later
    m_btnPrev->setEnabled(false);
    m_btnNext->setEnabled(false);
}

void TaskConfigDialog::loadConfig()
{
    QString cfgFile = m_taskDir + "/Settings.cfg";
    m_config = SettingsParser::parse(cfgFile);
    
    // Fallback defaults
    if (m_config.name.isEmpty()) {
        m_config.name = m_taskName;
        m_config.inputFile = m_taskName + ".INP";
        m_config.outputFile = m_taskName + ".OUT";
        m_config.useStdIn = false;
        m_config.useStdOut = false;
        m_config.evaluatorName = "C1LinesWordsIgnoreCase.dll";
        m_config.mark = 1.0;
        m_config.timeLimit = 1.0;
        m_config.memoryLimit = 1024.0;
    }

    m_editInpFile->setText(m_config.inputFile);
    m_editOutFile->setText(m_config.outputFile);
    m_chkStdIn->setChecked(m_config.useStdIn);
    m_chkStdOut->setChecked(m_config.useStdOut);
    
    int cIdx = m_cmbChecker->findText(m_config.evaluatorName);
    if (cIdx >= 0) m_cmbChecker->setCurrentIndex(cIdx);
    else {
        m_cmbChecker->addItem(m_config.evaluatorName);
        m_cmbChecker->setCurrentIndex(m_cmbChecker->count() - 1);
    }

    // Load tests
    QDir d(m_taskDir);
    QStringList testDirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    // Filter only those containing 'test' (case insensitive) or just list all
    QStringList validTests;
    for (const QString &td : testDirs) {
        if (td.toLower().contains("test")) validTests << td;
    }
    
    m_table->setRowCount(validTests.size() + 1);
    
    // Row 0: Global settings
    m_table->setVerticalHeaderItem(0, new QTableWidgetItem("Thiết lập chung"));
    m_table->setItem(0, 0, new QTableWidgetItem(QString::number(m_config.mark)));
    m_table->setItem(0, 1, new QTableWidgetItem(QString::number(m_config.timeLimit)));
    m_table->setItem(0, 2, new QTableWidgetItem(QString::number(m_config.memoryLimit)));

    // Rows 1..N: Individual tests
    for (int i = 0; i < validTests.size(); ++i) {
        QString tname = validTests[i];
        m_table->setVerticalHeaderItem(i + 1, new QTableWidgetItem(tname));
        
        // Find if this test has specific override in m_config
        QString markStr, timeStr, memStr;
        for (const TestCaseConfig &tc : m_config.testCases) {
            if (tc.name.toLower() == tname.toLower()) {
                if (tc.mark >= 0) markStr = QString::number(tc.mark);
                if (tc.timeLimit >= 0) timeStr = QString::number(tc.timeLimit);
                if (tc.memoryLimit >= 0) memStr = QString::number(tc.memoryLimit);
                break;
            }
        }
        m_table->setItem(i + 1, 0, new QTableWidgetItem(markStr));
        m_table->setItem(i + 1, 1, new QTableWidgetItem(timeStr));
        m_table->setItem(i + 1, 2, new QTableWidgetItem(memStr));
    }
}

void TaskConfigDialog::saveConfig()
{
    m_config.name = m_taskName;
    m_config.inputFile = m_editInpFile->text();
    m_config.outputFile = m_editOutFile->text();
    m_config.useStdIn = m_chkStdIn->isChecked();
    m_config.useStdOut = m_chkStdOut->isChecked();
    m_config.evaluatorName = m_cmbChecker->currentText();
    
    bool ok;
    double gm = m_table->item(0, 0)->text().toDouble(&ok); if (ok) m_config.mark = gm;
    double gt = m_table->item(0, 1)->text().toDouble(&ok); if (ok) m_config.timeLimit = gt;
    double gm2 = m_table->item(0, 2)->text().toDouble(&ok); if (ok) m_config.memoryLimit = gm2;
    
    m_config.testCases.clear();
    for (int i = 1; i < m_table->rowCount(); ++i) {
        TestCaseConfig tc;
        tc.name = m_table->verticalHeaderItem(i)->text();
        tc.mark = -1; tc.timeLimit = -1; tc.memoryLimit = -1;
        
        if (m_table->item(i, 0) && !m_table->item(i, 0)->text().isEmpty())
            tc.mark = m_table->item(i, 0)->text().toDouble();
        if (m_table->item(i, 1) && !m_table->item(i, 1)->text().isEmpty())
            tc.timeLimit = m_table->item(i, 1)->text().toDouble();
        if (m_table->item(i, 2) && !m_table->item(i, 2)->text().isEmpty())
            tc.memoryLimit = m_table->item(i, 2)->text().toDouble();
            
        if (tc.mark >= 0 || tc.timeLimit >= 0 || tc.memoryLimit >= 0) {
            m_config.testCases.append(tc);
        }
    }

    if (SettingsParser::save(m_taskDir + "/Settings.cfg", m_config)) {
        accept();
    } else {
        QMessageBox::critical(this, "Lỗi", "Không thể lưu cấu hình bài thi!");
    }
}

void TaskConfigDialog::onSave()
{
    saveConfig();
}

void TaskConfigDialog::onCancel()
{
    reject();
}

void TaskConfigDialog::onPrevTask()
{
}

void TaskConfigDialog::onNextTask()
{
}
