#include "JudgeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFont>

JudgeDialog::JudgeDialog(JudgeEngine *engine, QWidget *parent)
    : QDialog(parent), m_engine(engine)
{
    setWindowTitle("Đang chấm bài...");
    setMinimumSize(620, 400);
    buildUI();

    connect(engine, &JudgeEngine::updateReady,
            this,   &JudgeDialog::onUpdate);
    connect(engine, &JudgeEngine::progressChanged,
            this,   &JudgeDialog::onProgress);
    connect(engine, &JudgeEngine::logMessage,
            this,   &JudgeDialog::onLog);
    connect(engine, &JudgeEngine::finished,
            this,   &JudgeDialog::onFinished);
}

JudgeDialog::~JudgeDialog()
{
    if (m_engine && m_engine->isRunning())
        m_engine->stop();
}

void JudgeDialog::buildUI()
{
    QVBoxLayout *lay = new QVBoxLayout(this);

    m_lblStatus = new QLabel("Khởi động Judger...");
    lay->addWidget(m_lblStatus);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFormat("Đã chấm %v / %m thí sinh");
    lay->addWidget(m_progress);

    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setLineWrapMode(QTextEdit::NoWrap);
    QFont f("Monospace", 9);
    m_log->setFont(f);
    lay->addWidget(m_log, 1);

    QDialogButtonBox *box = new QDialogButtonBox();
    QPushButton *btnStop = box->addButton("⛔ Dừng lại", QDialogButtonBox::RejectRole);
    QPushButton *btnClose = box->addButton("✔ Đóng", QDialogButtonBox::AcceptRole);
    btnClose->setEnabled(false);

    connect(btnStop,  &QPushButton::clicked, this, [this, btnStop, btnClose]() {
        m_engine->stop();
        btnStop->setEnabled(false);
        btnClose->setEnabled(true);
        m_lblStatus->setText("⛔ Đã dừng.");
    });
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    // Store btnClose to enable when done
    btnClose->setObjectName("btnClose");
    btnStop->setObjectName("btnStop");

    lay->addWidget(box);
}

void JudgeDialog::onUpdate(const JudgeUpdate &u)
{
    // Accumulate scores
    m_scores[u.contestantName][u.taskName] = u.taskScore;

    // Color-coded log entry
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

    m_lblStatus->setText(QString("Đang chấm: %1 / %2")
        .arg(u.contestantName, u.taskName));
}

void JudgeDialog::onProgress(int done, int total)
{
    m_progress->setMaximum(total);
    m_progress->setValue(done);
}

void JudgeDialog::onLog(const QString &msg)
{
    // Already logged via onUpdate for most cases; this handles extra messages
    Q_UNUSED(msg);
}

void JudgeDialog::onFinished()
{
    m_lblStatus->setText("✔ Chấm bài hoàn tất!");
    setWindowTitle("Chấm bài hoàn tất");

    if (auto *btn = findChild<QPushButton*>("btnStop"))  btn->setEnabled(false);
    if (auto *btn = findChild<QPushButton*>("btnClose")) btn->setEnabled(true);

    m_log->append("<br/><b style='color:navy'>═══ Hoàn tất ═══</b>");

    emit judgeFinished(m_scores);
}
