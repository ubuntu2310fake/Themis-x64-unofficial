#include "JudgeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFont>
#include <QTime>
#include <QIcon>

JudgeDialog::JudgeDialog(JudgeEngine *engine, const QString &mode, QWidget *parent)
    : QDialog(parent), m_engine(engine), m_mode(mode)
{
    setWindowTitle("Chấm bài");
    buildUI();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &JudgeDialog::updateTimerDisplay);
    m_elapsed.start();
    m_timer->start(200); // 5 updates per second

    connect(engine, &JudgeEngine::updateReady,
            this,   &JudgeDialog::onUpdate);
    connect(engine, &JudgeEngine::statusChanged,
            this,   &JudgeDialog::onStatusChanged);
    connect(engine, &JudgeEngine::progressChanged,
            this,   &JudgeDialog::onProgress);
    connect(engine, &JudgeEngine::logMessage,
            this,   &JudgeDialog::onLog);
    connect(engine, &JudgeEngine::finished,
            this,   &JudgeDialog::onFinished);
}

JudgeDialog::~JudgeDialog()
{
    if (m_timer) m_timer->stop();
    if (m_engine && m_engine->isRunning())
        m_engine->stop();
}

void JudgeDialog::buildUI()
{
    if (m_mode == "detailed") {
        buildDetailedUI();
    } else {
        buildClassicUI();
    }
}

void JudgeDialog::buildClassicUI()
{
    setFixedSize(400, 150);
    setStyleSheet(
        "QDialog { background-color: #f0f0f0; }"
        "QProgressBar { border: 1px solid #707070; border-radius: 0px; background: #e6e6e6; height: 16px; }"
        "QProgressBar::chunk { background-color: #55d400; }"
        "QPushButton#btnStop { background-color: #ffd279; border: 1px solid #d4a034; border-radius: 4px; padding: 4px 14px; font-weight: bold; color: #333333; }"
        "QPushButton#btnStop:hover { background-color: #ffde94; border: 1px solid #e0b040; }"
        "QPushButton#btnStop:pressed { background-color: #f0c050; }"
    );

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 14, 18, 14);
    lay->setSpacing(6);

    // Row 1: Name / Task / Test
    m_lblName = new QLabel("Khởi động...", this);
    QFont fName = font();
    fName.setBold(true);
    fName.setPointSize(10);
    m_lblName->setFont(fName);
    lay->addWidget(m_lblName);

    // Row 2: Progress bar (Gauge)
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    lay->addWidget(m_progress);

    // Row 3: Status (Left) & Time (Right)
    QHBoxLayout *layInfo = new QHBoxLayout();
    m_lblStatus = new QLabel("Nạp bài làm...", this);
    m_lblStatus->setStyleSheet("color: #222222;");

    m_lblTime = new QLabel("Thời gian chấm: 00:00:00", this);
    m_lblTime->setStyleSheet("color: #222222;");
    layInfo->addWidget(m_lblStatus, 1);
    layInfo->addWidget(m_lblTime, 0, Qt::AlignRight);
    lay->addLayout(layInfo);

    // Row 4: Ngừng chấm button (Centered)
    QHBoxLayout *layBtn = new QHBoxLayout();
    layBtn->addStretch();

    QPushButton *btnStop = new QPushButton(" Ngừng chấm", this);
    btnStop->setObjectName("btnStop");
    btnStop->setIcon(QIcon(":/btnstopjudge.png"));
    btnStop->setIconSize(QSize(20, 20));
    btnStop->setCursor(Qt::PointingHandCursor);

    connect(btnStop, &QPushButton::clicked, this, [this, btnStop]() {
        if (m_engine && m_engine->isRunning()) {
            m_engine->stop();
            btnStop->setText(" Đã đóng");
            btnStop->setEnabled(false);
            m_lblStatus->setText("Đã ngừng chấm.");
            if (m_timer) m_timer->stop();
        } else {
            accept();
        }
    });

    layBtn->addWidget(btnStop);
    layBtn->addStretch();
    lay->addLayout(layBtn);
}

void JudgeDialog::buildDetailedUI()
{
    setMinimumSize(620, 420);
    setStyleSheet(
        "QProgressBar { border: 1px solid #707070; background: #e6e6e6; height: 18px; text-align: center; }"
        "QProgressBar::chunk { background-color: #28a745; }"
    );

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    QHBoxLayout *topLay = new QHBoxLayout();
    m_lblName = new QLabel("Khởi động Judger...", this);
    QFont fName = font();
    fName.setBold(true);
    m_lblName->setFont(fName);
    topLay->addWidget(m_lblName, 1);

    m_lblTime = new QLabel("Thời gian: 00:00:00", this);
    topLay->addWidget(m_lblTime);
    lay->addLayout(topLay);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFormat("Đã chấm %v / %m thí sinh");
    lay->addWidget(m_progress);

    m_lblStatus = new QLabel("Đang chuẩn bị...", this);
    lay->addWidget(m_lblStatus);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setLineWrapMode(QTextEdit::NoWrap);
    QFont f("Monospace", 9);
    m_log->setFont(f);
    lay->addWidget(m_log, 1);

    QDialogButtonBox *box = new QDialogButtonBox(this);
    QPushButton *btnStop = box->addButton("⛔ Ngừng chấm", QDialogButtonBox::RejectRole);
    QPushButton *btnClose = box->addButton("✔ Đóng", QDialogButtonBox::AcceptRole);
    btnClose->setEnabled(false);

    connect(btnStop, &QPushButton::clicked, this, [this, btnStop, btnClose]() {
        m_engine->stop();
        btnStop->setEnabled(false);
        btnClose->setEnabled(true);
        m_lblStatus->setText("⛔ Đã ngừng chấm.");
        if (m_timer) m_timer->stop();
    });
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnClose->setObjectName("btnClose");
    btnStop->setObjectName("btnStop");

    lay->addWidget(box);
}

void JudgeDialog::updateTimerDisplay()
{
    qint64 ms = m_elapsed.elapsed();
    int secs = (ms / 1000) % 60;
    int mins = (ms / 60000) % 60;
    int hrs  = (ms / 3600000);
    QString timeStr = QString("%1:%2:%3")
                          .arg(hrs, 2, 10, QChar('0'))
                          .arg(mins, 2, 10, QChar('0'))
                          .arg(secs, 2, 10, QChar('0'));

    if (m_lblTime) {
        if (m_mode == "detailed") {
            m_lblTime->setText("Thời gian: " + timeStr);
        } else {
            m_lblTime->setText("Thời gian chấm: " + timeStr);
        }
    }
}

void JudgeDialog::onStatusChanged(const QString &contestant, const QString &task, const QString &status)
{
    if (m_lblName) {
        m_lblName->setText(QString("%1-%2").arg(contestant, task));
    }
    if (m_lblStatus) {
        m_lblStatus->setText(status);
    }
}

void JudgeDialog::onUpdate(const JudgeUpdate &u)
{
    // Accumulate scores
    m_scores[u.contestantName][u.taskName] = u.taskScore;

    if (m_lblName) {
        if (u.testIndex >= 0) {
            m_lblName->setText(QString("%1-%2 (Test %3)").arg(u.contestantName, u.taskName).arg(u.testIndex + 1));
        } else {
            m_lblName->setText(QString("%1-%2").arg(u.contestantName, u.taskName));
        }
    }

    if (!u.statusText.isEmpty() && m_lblStatus) {
        m_lblStatus->setText(u.statusText);
    }

    // Color-coded log entry for detailed mode
    if (m_log) {
        QString color;
        switch (u.testResult.verdict) {
        case Verdict::AC:  color = "#006600"; break;
        case Verdict::WA:  color = "#cc0000"; break;
        case Verdict::TLE: color = "#cc6600"; break;
        case Verdict::MLE: color = "#aa00aa"; break;
        case Verdict::RTE: color = "#cc0000"; break;
        case Verdict::CE:  color = "#888800"; break;
        case Verdict::SK:  color = "#888888"; break;
        default:           color = "#333333"; break;
        }

        if (u.testIndex >= 0) {
            m_log->append(
                QString("<span style='color:%1'>[%2 / %3 / Test%4] %5  "
                        "<b>%6ms</b>  %7KB  <b>%8</b></span>")
                .arg(color, u.contestantName, u.taskName)
                .arg(u.testIndex + 1)
                .arg(verdictText(u.testResult.verdict))
                .arg(u.testResult.timeMs, 0, 'f', 1)
                .arg(u.testResult.memKb)
                .arg(u.testResult.score, 0, 'f', 2));
        } else {
            // Task-level result (CE, SK, IE)
            QString msg = u.testResult.checkerMsg;
            if (!msg.isEmpty()) msg = " – " + msg;
            
            m_log->append(
                QString("<span style='color:%1'><b>[%2 / %3] %4</b>%5</span>")
                .arg(color, u.contestantName, u.taskName,
                     verdictText(u.testResult.verdict), msg));
        }
    }
}

void JudgeDialog::onProgress(int done, int total)
{
    if (m_progress) {
        m_progress->setMaximum(total);
        m_progress->setValue(done);
    }
}

void JudgeDialog::onLog(const QString &msg)
{
    Q_UNUSED(msg);
}

void JudgeDialog::onFinished()
{
    if (m_timer) m_timer->stop();

    if (m_lblStatus) {
        m_lblStatus->setText("✔ Chấm bài hoàn tất!");
    }
    if (m_lblName) {
        m_lblName->setText("Hoàn tất kỳ thi");
    }
    setWindowTitle("Chấm bài hoàn tất");

    auto *btnStop = findChild<QPushButton*>("btnStop");
    auto *btnClose = findChild<QPushButton*>("btnClose");

    if (m_mode == "classic") {
        if (btnStop) {
            btnStop->setText(" Đóng");
            btnStop->setIcon(QIcon());
            btnStop->setEnabled(true);
            btnStop->setFocus();
        }
    } else {
        if (btnStop)  btnStop->setEnabled(false);
        if (btnClose) {
            btnClose->setEnabled(true);
            btnClose->setFocus();
        }
        if (m_log) {
            m_log->append("<br/><b style='color:navy'>═══ Hoàn tất ═══</b>");
        }
    }

    emit judgeFinished(m_scores);
}
