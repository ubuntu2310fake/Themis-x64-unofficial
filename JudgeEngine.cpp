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
#include <QCollator>
#include <algorithm>

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

    res.sourceFile = QFileInfo(sourceFile).fileName();

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

    // Filter test folders: first try from cfg.testCases, else scan testFolders
    QStringList tests;
    if (!cfg.testCases.isEmpty()) {
        for (const TestCaseConfig &tc : cfg.testCases) {
            for (const QString &f : testFolders) {
                if (f.compare(tc.name, Qt::CaseInsensitive) == 0) {
                    if (!tests.contains(f)) tests << f;
                    break;
                }
            }
        }
    }
    if (tests.isEmpty()) {
        for (const QString &f : testFolders) {
            if (f.startsWith("test", Qt::CaseInsensitive)) {
                tests << f;
            } else {
                QDir sub(taskDir.filePath(f));
                QStringList fs = sub.entryList(QDir::Files);
                bool hasTestFiles = false;
                for (const QString &fileName : fs) {
                    if (fileName.endsWith(".inp", Qt::CaseInsensitive) ||
                        fileName.endsWith(".in", Qt::CaseInsensitive) ||
                        fileName.endsWith(".out", Qt::CaseInsensitive) ||
                        fileName.endsWith(".ans", Qt::CaseInsensitive) ||
                        fileName.endsWith(".txt", Qt::CaseInsensitive)) {
                        hasTestFiles = true;
                        break;
                    }
                }
                if (hasTestFiles) tests << f;
            }
        }
    }

    // Natural sort tests (Test1, Test2, ..., Test10)
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(tests.begin(), tests.end(), collator);

    for (int ti = 0; ti < tests.size(); ++ti) {
        const QString &testName = tests[ti];
        QString testPath = taskDir.filePath(testName);
        QDir td(testPath);

        // Find INP and OUT files case-insensitively
        QString inpFile;
        if (!cfg.inputFile.isEmpty()) {
            for (const QString &f : td.entryList(QDir::Files)) {
                if (f.compare(cfg.inputFile, Qt::CaseInsensitive) == 0) {
                    inpFile = td.filePath(f);
                    break;
                }
            }
        }
        if (inpFile.isEmpty()) {
            for (const QString &f : td.entryList(QDir::Files)) {
                if (f.endsWith(".inp", Qt::CaseInsensitive) || f.endsWith(".in", Qt::CaseInsensitive)) {
                    inpFile = td.filePath(f);
                    break;
                }
            }
        }

        QString outFile;
        if (!cfg.outputFile.isEmpty()) {
            for (const QString &f : td.entryList(QDir::Files)) {
                if (f.compare(cfg.outputFile, Qt::CaseInsensitive) == 0) {
                    outFile = td.filePath(f);
                    break;
                }
            }
        }
        if (outFile.isEmpty()) {
            for (const QString &f : td.entryList(QDir::Files)) {
                if (f.endsWith(".out", Qt::CaseInsensitive) || f.endsWith(".ans", Qt::CaseInsensitive)) {
                    outFile = td.filePath(f);
                    break;
                }
            }
        }

        // Copy INP to working directory with all possible casings so freopen finds it
        QString baseInp = !cfg.inputFile.isEmpty() ? QFileInfo(cfg.inputFile).completeBaseName() : taskName;
        QStringList inpVariations = {
            cfg.inputFile,
            baseInp + ".INP",
            baseInp + ".inp",
            baseInp.toUpper() + ".INP",
            baseInp.toLower() + ".inp",
            taskName + ".INP",
            taskName + ".inp",
            taskName.toUpper() + ".INP",
            taskName.toLower() + ".inp"
        };
        inpVariations.removeDuplicates();
        for (const QString &v : inpVariations) {
            if (v.isEmpty()) continue;
            QString target = workPath + "/" + v;
            QFile::remove(target);
            if (!inpFile.isEmpty()) {
                QFile::copy(inpFile, target);
            }
        }

        // Prepare output file target
        QString defaultOutName = !cfg.outputFile.isEmpty() ? cfg.outputFile : (taskName + ".OUT");
        QString actualOut = cfg.useStdOut ? (workPath + "/" + testName + ".out") : (workPath + "/" + defaultOutName);
        QFile::remove(actualOut);
        // Also remove any existing case variations of output in workPath
        QFile::remove(workPath + "/" + taskName + ".out");
        QFile::remove(workPath + "/" + taskName + ".OUT");
        QFile::remove(workPath + "/" + taskName.toLower() + ".out");
        QFile::remove(workPath + "/" + taskName.toUpper() + ".OUT");

        // Determine test limits
        double timeLimit = cfg.timeLimit > 0 ? cfg.timeLimit : 1.0;
        int    memLimit  = cfg.memoryLimit > 0 ? (int)cfg.memoryLimit : 1024;

        // Check per-test overrides
        for (const TestCaseConfig &tc : cfg.testCases) {
            if (tc.name.compare(testName, Qt::CaseInsensitive) == 0) {
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
        tr.testName = testName;

        // If useStdOut is false, search workPath case-insensitively for the contestant output!
        if (!cfg.useStdOut) {
            QDir wd(workPath);
            QString foundOut;
            QString targetBase = !cfg.outputFile.isEmpty() ? QFileInfo(cfg.outputFile).completeBaseName() : taskName;
            for (const QString &f : wd.entryList(QDir::Files)) {
                if (f.compare(defaultOutName, Qt::CaseInsensitive) == 0 ||
                    f.compare(targetBase + ".out", Qt::CaseInsensitive) == 0 ||
                    f.compare(taskName + ".out", Qt::CaseInsensitive) == 0) {
                    foundOut = wd.filePath(f);
                    break;
                }
            }
            if (!foundOut.isEmpty()) {
                actualOut = foundOut;
            }
        }

        // Determine score for this test
        double testMark = cfg.mark > 0 ? cfg.mark : 1.0;
        for (const TestCaseConfig &tc : cfg.testCases) {
            if (tc.name.compare(testName, Qt::CaseInsensitive) == 0 && tc.mark > 0) {
                testMark = tc.mark;
                break;
            }
        }

        // Check output if ran successfully
        if (tr.verdict == Verdict::Judging) {
            QString msg;
            double ratio = Judger::checkOutput(outFile, actualOut, checker,
                                               "", testPath, workPath, msg);
            tr.checkerMsg = msg;
            tr.score      = ratio * testMark;
            if (ratio >= 1.0) {
                tr.verdict = Verdict::AC;
            } else {
                tr.verdict = Verdict::WA;
            }
        } else if (tr.verdict == Verdict::TLE) {
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Chạy quá thời gian";
        } else if (tr.verdict == Verdict::MLE) {
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Chạy quá bộ nhớ";
        } else if (tr.verdict == Verdict::RTE) {
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Chạy sinh lỗi";
        } else {
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Lỗi nghiêm trọng";
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

        emit logMessage(QString("[%1/%2/%3] %4  %5s  %6KB  %7 pts")
            .arg(m_cfg.contestantName, taskName, testName,
                 verdictText(tr.verdict))
            .arg(tr.timeMs / 1000.0, 0, 'f', 3)
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
    QString logFilePath = m_cfg.contestantsDir + "/Logs/" + name + ".log";

    // Read existing log to preserve tasks that were not judged in this run!
    QMap<QString, QStringList> taskBlocks;
    QStringList taskOrder;

    QFile existingFile(logFilePath);
    if (existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&existingFile);
        QString currentTask;
        while (!in.atEnd()) {
            QString line = in.readLine();
            QString trimmed = line.trimmed();
            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                currentTask = trimmed.mid(1, trimmed.length() - 2);
                if (!taskOrder.contains(currentTask)) taskOrder.append(currentTask);
                taskBlocks[currentTask].clear();
            } else if (!currentTask.isEmpty()) {
                taskBlocks[currentTask].append(line);
            }
        }
        existingFile.close();
    }

    // Now update or add the new results
    for (const TaskResult &tr : results) {
        if (!taskOrder.contains(tr.taskName)) taskOrder.append(tr.taskName);
        QStringList lines;
        if (!tr.sourceFile.isEmpty()) {
            lines.append("Source=" + tr.sourceFile);
        }
        lines.append(QString("Score=%1").arg(tr.totalScore, 0, 'f', 2));
        if (!tr.ceLog.isEmpty()) {
            lines.append("CE");
            lines.append(tr.ceLog);
        } else if (tr.verdict == Verdict::SK) {
            lines.append("SK");
        } else {
            lines.append("OK");
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
                // Format: testName|verdict|timeSec|memKb|score|checkerMsg
                lines.append(QString("%1|%2|%3|%4|%5|%6")
                    .arg(tc.testName)
                    .arg(vStr)
                    .arg(QString::number(tc.timeMs / 1000.0, 'f', 6))
                    .arg(tc.memKb)
                    .arg(QString::number(tc.score, 'f', 2))
                    .arg(tc.checkerMsg));
            }
        }
        taskBlocks[tr.taskName] = lines;
    }

    // Write back all tasks to logFile
    QFile logFile(logFilePath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&logFile);
        for (const QString &tname : taskOrder) {
            out << "[" << tname << "]\n";
            for (const QString &l : taskBlocks[tname]) {
                out << l << "\n";
            }
        }
        logFile.close();
    }

    ++m_doneCount;
    emit progressChanged(m_doneCount, m_total);
    if (m_doneCount >= m_total)
        emit finished();
}
