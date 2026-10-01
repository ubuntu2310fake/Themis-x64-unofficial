#include "JudgeEngine.h"
#include "Judger.h"
#include "SettingsParser.h"
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QDebug>
#include <QProcess>
#include <QElapsedTimer>
#include <QApplication>
#include <QTextStream>

// ═══════════════════════════════════════════════════════════════════════════════
// JudgeWorker
// ═══════════════════════════════════════════════════════════════════════════════

JudgeWorker::JudgeWorker(const Config &cfg, QObject *parent)
    : QThread(parent), m_cfg(cfg) {}

void JudgeWorker::run()
{
    QList<TaskResult> results;
    for (const QString &task : m_cfg.taskNames)
        results << judgeTask(task);
    emit contestantDone(m_cfg.contestantName, results);
}

// ─── Find source file for a task (Case Insensitive for Windows compatibility) ──
QString JudgeWorker::findSourceFile(const QString &taskName, QString &foundExt) const
{
    QDir dir(m_cfg.contestantDir + "/" + m_cfg.contestantName);
    if (!dir.exists()) return {};

    QStringList files = dir.entryList(QDir::Files);

    // Try each compiler entry in order
    for (const CompilerEntry &e : m_cfg.compilers) {
        QString expectedName = e.ext.isEmpty() ? taskName : (taskName + e.ext);
        
        for (const QString &f : files) {
            if (f.compare(expectedName, Qt::CaseInsensitive) == 0) {
                foundExt = e.ext;
                return dir.filePath(f);
            }
        }
    }
    return {};
}

// ─── Find compiler entry for extension ────────────────────────────────────────
const CompilerEntry* JudgeWorker::findCompiler(const QString &ext) const
{
    for (const CompilerEntry &e : m_cfg.compilers)
        if (e.ext == ext) return &e;
    return nullptr;
}

// ─── Judge one task for one contestant ────────────────────────────────────────
TaskResult JudgeWorker::judgeTask(const QString &taskName)
{
    TaskResult res;
    res.taskName = taskName;

    emit logMessage(QString("[%1] Judging task %2...").arg(m_cfg.contestantName, taskName));

    // ── Load task config ──────────────────────────────────────────────────────
    QString settingsCfg = m_cfg.tasksDir + "/" + taskName + "/Settings.cfg";
    TaskConfig cfg = SettingsParser::parse(settingsCfg);
    if (cfg.name.isEmpty()) {
        // Fallback to default if Settings.cfg is missing
        cfg.name          = taskName;
        cfg.inputFile     = taskName + ".INP";
        cfg.outputFile    = taskName + ".OUT";
        cfg.useStdIn      = false;
        cfg.useStdOut     = false;
        cfg.evaluatorName = "C1LinesWordsIgnoreCase.dll";
        cfg.mark          = 1.0;
        cfg.timeLimit     = 1.0;
        cfg.memoryLimit   = 1024.0;
        emit logMessage(QString("[%1] Không có Settings.cfg, dùng mặc định (1đ, 1s, 1024MB)").arg(taskName));
    }

    // ── Find source file ──────────────────────────────────────────────────────
    QString foundExt;
    QString sourceFile = findSourceFile(taskName, foundExt);

    if (sourceFile.isEmpty()) {
        res.verdict = Verdict::SK;
        JudgeUpdate u; u.contestantName = m_cfg.contestantName;
        u.taskName = taskName; u.testIndex = -1; u.taskScore = 0;
        u.testResult.verdict = Verdict::SK;
        u.testResult.checkerMsg = "Không tìm thấy file mã nguồn";
        emit updateReady(u);
        emit logMessage(QString("[%1/%2] Không nộp bài").arg(m_cfg.contestantName, taskName));
        return res;
    }

    // ── Create working directory ──────────────────────────────────────────────
    QString workPath = m_cfg.workDir + "/" + m_cfg.contestantName + "/" + taskName;
    QDir().mkpath(workPath);

    // Copy source to workdir
    QString localSrc = workPath + "/" + taskName + foundExt;
    QFile::remove(localSrc);
    QFile::copy(sourceFile, localSrc);

    // ── Compile ───────────────────────────────────────────────────────────────
    const CompilerEntry *compiler = findCompiler(foundExt);
    QString exeFile;
    bool isInterpreted = false;

    if (foundExt == ".py") {
        // Python: run via interpreter directly
        exeFile      = localSrc;  // the .py file IS the "executable"
        isInterpreted = true;
    } else if (foundExt.isEmpty()) {
        // ELF: make executable and run directly
        exeFile = localSrc;
#ifndef Q_OS_WIN
        QProcess::execute("chmod", {"+x", exeFile});
#endif
    } else {
        // Need to compile
        exeFile = workPath + "/" + taskName;
#ifdef Q_OS_WIN
        exeFile += ".exe";
#endif
        QString ceLog;
        bool ok = compiler
            ? Judger::compile(localSrc, exeFile, *compiler, taskName, ceLog)
            : false;

        if (!ok) {
            res.verdict = Verdict::CE;
            res.ceLog   = ceLog.isEmpty() ? "Compilation failed (no compiler found)" : ceLog;
            JudgeUpdate u; u.contestantName = m_cfg.contestantName;
            u.taskName = taskName; u.testIndex = -1; u.taskScore = 0;
            u.testResult.verdict = Verdict::CE;
            u.testResult.checkerMsg = res.ceLog.split('\n').first(); // just first line for log
            emit updateReady(u);
            emit logMessage(QString("[%1/%2] CE: %3").arg(m_cfg.contestantName, taskName, res.ceLog.left(80)));
            return res;
        }
        // Set executable permission
#ifndef Q_OS_WIN
        QProcess::execute("chmod", {"+x", exeFile});
#endif
    }

    // ── Judge each test case ──────────────────────────────────────────────────
    CheckerType checker = checkerFromName(cfg.evaluatorName);
    double totalScore   = 0.0;
    bool allAC          = true;

    QDir taskDir(m_cfg.tasksDir + "/" + taskName);
    QStringList testFolders = taskDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    // Filter out non-test folders
    QStringList tests;
    for (const QString &f : testFolders)
        if (f.startsWith("Test", Qt::CaseInsensitive) || f.startsWith("test"))
            tests << f;

    for (int ti = 0; ti < tests.size(); ++ti) {
        const QString &testName = tests[ti];
        QString testPath = taskDir.filePath(testName);

        // Find INP and OUT files
        QString inpFile = testPath + "/" + cfg.inputFile;
        QString outFile = testPath + "/" + cfg.outputFile;

        if (!QFile::exists(inpFile) || !QFile::exists(outFile)) {
            // Try to find by name pattern
            QDir td(testPath);
            QStringList inps = td.entryList({"*.INP","*.inp"}, QDir::Files);
            QStringList outs = td.entryList({"*.OUT","*.out"}, QDir::Files);
            if (!inps.isEmpty()) inpFile = testPath + "/" + inps.first();
            if (!outs.isEmpty()) outFile = testPath + "/" + outs.first();
        }

        // Copy INP to working directory so freopen can find it
        QString localInp = workPath + "/" + cfg.inputFile;
        QFile::remove(localInp);
        QFile::copy(inpFile, localInp);

        // If useStdOut is false, program writes to cfg.outputFile in workDir
        QString actualOut = cfg.useStdOut ? (workPath + "/" + testName + ".out") : (workPath + "/" + cfg.outputFile);
        QFile::remove(actualOut);

        // Determine test limits
        double timeLimit = cfg.timeLimit;
        int    memLimit  = cfg.memoryLimit;

        // Check per-test overrides
        for (const TestCaseConfig &tc : cfg.testCases) {
            if (tc.name == testName) {
                if (tc.timeLimit  > 0) timeLimit = tc.timeLimit;
                if (tc.memoryLimit > 0) memLimit = tc.memoryLimit;
                break;
            }
        }

        // Run
        TestResult tr;
        tr.testName = testName;

        if (isInterpreted && foundExt == ".py") {
            QString interpreter = compiler ? compiler->command : "/usr/bin/python3";
            tr = Judger::execute(exeFile, workPath, inpFile, actualOut, cfg.useStdIn, cfg.useStdOut, timeLimit, memLimit, interpreter);
        } else {
            tr = Judger::execute(exeFile, workPath, inpFile, actualOut, cfg.useStdIn, cfg.useStdOut, timeLimit, memLimit, "");
        }

        // Check output if ran successfully
        if (tr.verdict == Verdict::Judging) {
            QString msg;
            double ratio = Judger::checkOutput(outFile, actualOut, checker,
                                               "", testPath, workPath, msg);
            tr.checkerMsg = msg;
            tr.verdict    = (ratio >= 1.0) ? Verdict::AC : Verdict::WA;

            // Score for this test
            double testMark = cfg.mark; // default
            for (const TestCaseConfig &tc : cfg.testCases) {
                if (tc.name == testName && tc.mark > 0) { testMark = tc.mark; break; }
            }
            tr.score = ratio * testMark;
        }

        if (tr.verdict != Verdict::AC) allAC = false;
        totalScore += tr.score;
        res.tests << tr;

        // Emit per-test update
        JudgeUpdate u;
        u.contestantName = m_cfg.contestantName;
        u.taskName  = taskName;
        u.testIndex = ti;
        u.testResult = tr;
        u.taskScore = totalScore;
        emit updateReady(u);

        emit logMessage(QString("[%1/%2/%3] %4  %5ms  %6KB  %7 pts")
            .arg(m_cfg.contestantName, taskName, testName,
                 verdictText(tr.verdict))
            .arg(tr.timeMs, 0, 'f', 1)
            .arg(tr.memKb)
            .arg(tr.score, 0, 'f', 2));
    }

    res.totalScore = totalScore;
    res.verdict    = tests.isEmpty() ? Verdict::IE
                   : allAC           ? Verdict::AC
                                     : Verdict::WA;
    return res;
}

// ═══════════════════════════════════════════════════════════════════════════════
// JudgeEngine
// ═══════════════════════════════════════════════════════════════════════════════

JudgeEngine::JudgeEngine(const ContestConfig &cfg, QObject *parent)
    : QObject(parent), m_cfg(cfg)
{
    m_total = cfg.contestantNames.size();

    // Create temp work dir
    m_tempDir = QDir::tempPath() + "/ThemisLinux_" +
                QString::number(QApplication::applicationPid());
    QDir().mkpath(m_tempDir);
}

JudgeEngine::~JudgeEngine()
{
    stop();
    // Clean up temp dir
    QDir(m_tempDir).removeRecursively();
}

void JudgeEngine::start()
{
    m_doneCount = 0;
    m_stopped   = false;

    for (const QString &name : m_cfg.contestantNames) {
        JudgeWorker::Config wcfg;
        wcfg.contestantDir  = m_cfg.contestantsDir;
        wcfg.contestantName = name;
        wcfg.tasksDir       = m_cfg.tasksDir;
        wcfg.taskNames      = m_cfg.taskNames;
        wcfg.compilers      = m_cfg.compilers;
        wcfg.workDir        = m_tempDir;

        auto *w = new JudgeWorker(wcfg, this);
        connect(w,    &JudgeWorker::updateReady,
                this, &JudgeEngine::updateReady);
        connect(w,    &JudgeWorker::logMessage,
                this, &JudgeEngine::logMessage);
        connect(w,    &JudgeWorker::contestantDone,
                this, &JudgeEngine::onWorkerDone);

        m_workers << w;
        w->start();
    }

    if (m_cfg.contestantNames.isEmpty())
        emit finished();
}

void JudgeEngine::stop()
{
    m_stopped = true;
    for (auto *w : m_workers) {
        w->quit();
        w->wait(3000);
        w->deleteLater();
    }
    m_workers.clear();
}

bool JudgeEngine::isRunning() const
{
    for (auto *w : m_workers)
        if (w->isRunning()) return true;
    return false;
}

void JudgeEngine::onWorkerDone(const QString &name, const QList<TaskResult> &results)
{
    // Write detailed log to ContestantsDir/Logs/<name>.log
    QDir d(m_cfg.contestantsDir);
    d.mkdir("Logs");
    QFile logFile(m_cfg.contestantsDir + "/Logs/" + name + ".log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&logFile);
        for (const TaskResult &tr : results) {
            out << "[" << tr.taskName << "]\n";
            if (!tr.ceLog.isEmpty()) {
                out << "CE\n" << tr.ceLog << "\n";
            } else if (tr.verdict == Verdict::SK) {
                out << "SK\n";
            } else {
                for (const TestResult &tc : tr.tests) {
                    QString vStr;
                    switch(tc.verdict) {
                        case Verdict::AC: vStr = "AC"; break;
                        case Verdict::WA: vStr = "WA"; break;
                        case Verdict::TLE: vStr = "TLE"; break;
                        case Verdict::MLE: vStr = "MLE"; break;
                        case Verdict::RTE: vStr = "RTE"; break;
                        case Verdict::CE: vStr = "CE"; break;
                        case Verdict::SK: vStr = "SK"; break;
                        case Verdict::IE: vStr = "IE"; break;
                        default: vStr = "UNK"; break;
                    }
                    out << tc.testName << "|" << vStr << "|" << tc.timeMs << "|" << tc.memKb << "|" << tc.checkerMsg << "\n";
                }
            }
        }
        logFile.close();
    }

    ++m_doneCount;
    emit progressChanged(m_doneCount, m_total);
    if (m_doneCount >= m_total)
        emit finished();
}
