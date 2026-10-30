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
#include <QRegularExpression>
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

// ─── Source IO Detection (Linux Case Insensitivity & freopen adapter) ────────
struct DetectedIO {
    QString input;
    QString output;
};

static QString stripCodeComments(const QString &source, bool isPython)
{
    QString result;
    result.reserve(source.size());
    int n = source.size();
    int i = 0;
    while (i < n) {
        // Python docstrings / triple quote comments
        if (isPython && (source.mid(i, 3) == "\"\"\"" || source.mid(i, 3) == "'''")) {
            QString triple = source.mid(i, 3);
            i += 3;
            while (i + 2 < n && source.mid(i, 3) != triple) {
                if (source[i] == '\n') result.append('\n');
                i++;
            }
            if (i + 2 < n) i += 3;
            else i = n;
            result.append(' ');
        }
        // String literal "..."
        else if (source[i] == '"') {
            result.append(source[i++]);
            while (i < n && source[i] != '"') {
                if (source[i] == '\\' && i + 1 < n) {
                    result.append(source[i++]);
                }
                result.append(source[i++]);
            }
            if (i < n) result.append(source[i++]);
        }
        // Character literal '...'
        else if (source[i] == '\'') {
            result.append(source[i++]);
            while (i < n && source[i] != '\'') {
                if (source[i] == '\\' && i + 1 < n) {
                    result.append(source[i++]);
                }
                result.append(source[i++]);
            }
            if (i < n) result.append(source[i++]);
        }
        // Python comment #
        else if (isPython && source[i] == '#') {
            while (i < n && source[i] != '\n') {
                i++;
            }
            result.append(' ');
        }
        // C/C++ line comment //
        else if (i + 1 < n && source[i] == '/' && source[i+1] == '/') {
            while (i < n && source[i] != '\n') {
                i++;
            }
            result.append(' ');
        }
        // C/C++ block comment /* ... */
        else if (i + 1 < n && source[i] == '/' && source[i+1] == '*') {
            i += 2;
            while (i + 1 < n && !(source[i] == '*' && source[i+1] == '/')) {
                if (source[i] == '\n') result.append('\n');
                i++;
            }
            if (i + 1 < n) {
                i += 2;
            } else {
                i = n;
            }
            result.append(' ');
        }
        // Pascal block comment { ... }
        else if (source[i] == '{') {
            i++;
            while (i < n && source[i] != '}') {
                if (source[i] == '\n') result.append('\n');
                i++;
            }
            if (i < n) i++;
            result.append(' ');
        }
        // Pascal block comment (* ... *)
        else if (i + 1 < n && source[i] == '(' && source[i+1] == '*') {
            i += 2;
            while (i + 1 < n && !(source[i] == '*' && source[i+1] == ')')) {
                if (source[i] == '\n') result.append('\n');
                i++;
            }
            if (i + 1 < n) i += 2;
            else i = n;
            result.append(' ');
        }
        else {
            result.append(source[i++]);
        }
    }
    return result;
}

static DetectedIO detectIOFromSource(const QString &sourcePath)
{
    DetectedIO res;
    QFile f(sourcePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return res;
    }
    // Read up to 128KB of source code
    QString rawCode = QString::fromUtf8(f.read(131072));
    f.close();

    bool isPy = sourcePath.endsWith(".py", Qt::CaseInsensitive);
    QString code = stripCodeComments(rawCode, isPy);

    // Replace #define TASK "..." macros if present
    QRegularExpression reMacro(R"(#define\s+(\w+)\s+["']([^"']+)["'])");
    auto macroIter = reMacro.globalMatch(code);
    while (macroIter.hasNext()) {
        auto m = macroIter.next();
        QString name = m.captured(1);
        QString val = m.captured(2);
        code.replace(QRegularExpression("\\b" + QRegularExpression::escape(name) + R"(\s*"\.)"), "\"" + val + ".");
        code.replace(QRegularExpression(R"(\(\s*)" + QRegularExpression::escape(name) + R"(\s*\+\s*["']\.([^"']+)["']\s*\)\s*\.\s*c_str\s*\(\s*\))"),
                     "\"" + val + ".\\1\"");
    }

    // Also check: const string task = "...";
    QRegularExpression reConstStr(R"((?:const\s+)?(?:string|char\*)\s+(\w+)\s*=\s*["']([^"']+)["'])");
    auto constIter = reConstStr.globalMatch(code);
    while (constIter.hasNext()) {
        auto m = constIter.next();
        QString name = m.captured(1);
        QString val = m.captured(2);
        code.replace(QRegularExpression("\\b" + QRegularExpression::escape(name) + R"(\s*"\.)"), "\"" + val + ".");
        code.replace(QRegularExpression(R"(\(\s*)" + QRegularExpression::escape(name) + R"(\s*\+\s*["']\.([^"']+)["']\s*\)\s*\.\s*c_str\s*\(\s*\))"),
                     "\"" + val + ".\\1\"");
    }

    // 1. freopen with "r" or 'r' (input)
    // Examples: freopen("task.inp", "r", stdin); freopen("task.INP", "rt", stdin);
    QRegularExpression reFreopenIn(R"(freopen\s*\(\s*["']([^"']+)["']\s*,\s*["'][rR][tT]?["']\s*,\s*stdin\s*\))");
    auto match = reFreopenIn.match(code);
    if (match.hasMatch()) {
        res.input = match.captured(1).trimmed();
    }

    // 2. freopen with "w" or 'w' (output)
    // Examples: freopen("task.out", "w", stdout); freopen("task.OUT", "wt", stdout);
    QRegularExpression reFreopenOut(R"(freopen\s*\(\s*["']([^"']+)["']\s*,\s*["'][wW][tT]?["']\s*,\s*stdout\s*\))");
    match = reFreopenOut.match(code);
    if (match.hasMatch()) {
        res.output = match.captured(1).trimmed();
    }

    // 3. Pascal assign: assign(f, 'task.inp'); reset(f); or assign(output, 'task.out'); rewrite(output);
    if (res.input.isEmpty()) {
        QRegularExpression reAssignIn(R"(assign\s*\(\s*\w+\s*,\s*['"]([^'"]+\.(?:inp|in|txt))['"]\s*\))", QRegularExpression::CaseInsensitiveOption);
        match = reAssignIn.match(code);
        if (match.hasMatch()) {
            res.input = match.captured(1).trimmed();
        }
    }
    if (res.output.isEmpty()) {
        QRegularExpression reAssignOut(R"(assign\s*\(\s*\w+\s*,\s*['"]([^'"]+\.(?:out|ans|txt))['"]\s*\))", QRegularExpression::CaseInsensitiveOption);
        match = reAssignOut.match(code);
        if (match.hasMatch()) {
            res.output = match.captured(1).trimmed();
        }
    }

    // 4. C++ ifstream / ofstream
    if (res.input.isEmpty()) {
        QRegularExpression reIfstream(R"((?:ifstream\s+\w+\s*\(|(?:fin|cin|fi)\.open\s*\()\s*["']([^"']+)["'])");
        match = reIfstream.match(code);
        if (match.hasMatch()) {
            res.input = match.captured(1).trimmed();
        }
    }
    if (res.output.isEmpty()) {
        QRegularExpression reOfstream(R"((?:ofstream\s+\w+\s*\(|(?:fout|cout|fo)\.open\s*\()\s*["']([^"']+)["'])");
        match = reOfstream.match(code);
        if (match.hasMatch()) {
            res.output = match.captured(1).trimmed();
        }
    }

    // 5. Python open
    if (res.input.isEmpty()) {
        QRegularExpression rePyIn(R"((?:sys\.stdin\s*=\s*)?open\s*\(\s*["']([^"']+\.(?:inp|in|txt))["'](?:\s*,\s*["'][rR]["'])?\s*\))");
        match = rePyIn.match(code);
        if (match.hasMatch()) {
            res.input = match.captured(1).trimmed();
        }
    }
    if (res.output.isEmpty()) {
        QRegularExpression rePyOut(R"((?:sys\.stdout\s*=\s*)?open\s*\(\s*["']([^"']+\.(?:out|ans|txt))["']\s*,\s*["'][wW]["']\s*\))");
        match = rePyOut.match(code);
        if (match.hasMatch()) {
            res.output = match.captured(1).trimmed();
        }
    }

    // 6. Generic string literal fallback for .inp and .out if still not found
    if (res.input.isEmpty()) {
        QRegularExpression reGenIn(R"(["']([^"']+\.(?:inp|in))["'])", QRegularExpression::CaseInsensitiveOption);
        match = reGenIn.match(code);
        if (match.hasMatch()) {
            res.input = match.captured(1).trimmed();
        }
    }
    if (res.output.isEmpty()) {
        QRegularExpression reGenOut(R"(["']([^"']+\.(?:out|ans))["'])", QRegularExpression::CaseInsensitiveOption);
        match = reGenOut.match(code);
        if (match.hasMatch()) {
            res.output = match.captured(1).trimmed();
        }
    }

    if (!res.input.isEmpty()) res.input = QFileInfo(res.input).fileName();
    if (!res.output.isEmpty()) res.output = QFileInfo(res.output).fileName();

    return res;
}

// ─── External Checker Finder & Builder ────────────────────────────────────────
static QString findOrBuildChecker(const QString &taskDir, const QString &taskName, CheckerType checkerType)
{
    QDir td(taskDir);
    if (!td.exists()) return {};

    QStringList baseNames;
    if (checkerType == CheckerType::C7External) {
        baseNames << ("Check" + taskName)
                  << ("check" + taskName)
                  << ("Check" + taskName.toLower())
                  << ("check" + taskName.toLower())
                  << "checker"
                  << "check"
                  << "chk";
    } else {
        baseNames << taskName
                  << taskName.toLower()
                  << taskName.toUpper();
    }

    // Try finding existing binary, script, or executable
    for (const QString &base : baseNames) {
        // 1. Native ELF or extensionless executable
        QString noExt = td.filePath(base);
        if (QFile::exists(noExt)) {
            QFileInfo fi(noExt);
            if (fi.isFile()) {
                QFile::setPermissions(noExt, QFile::permissions(noExt) |
                    QFileDevice::ExeOwner | QFileDevice::ExeUser |
                    QFileDevice::ExeGroup | QFileDevice::ExeOther);
                return noExt;
            }
        }
        // 2. .exe
        QString exePath = td.filePath(base + ".exe");
        if (QFile::exists(exePath)) return exePath;

        // 3. .py
        QString pyPath = td.filePath(base + ".py");
        if (QFile::exists(pyPath)) return pyPath;

        // 4. .sh
        QString shPath = td.filePath(base + ".sh");
        if (QFile::exists(shPath)) {
            QFile::setPermissions(shPath, QFile::permissions(shPath) |
                QFileDevice::ExeOwner | QFileDevice::ExeUser |
                QFileDevice::ExeGroup | QFileDevice::ExeOther);
            return shPath;
        }

        // 5. .cpp source -> auto compile!
        QString cppPath = td.filePath(base + ".cpp");
        if (QFile::exists(cppPath)) {
            QString binPath = td.filePath(base + "_linux");
            QFileInfo fiCpp(cppPath);
            QFileInfo fiBin(binPath);
            if (!fiBin.exists() || fiBin.lastModified() < fiCpp.lastModified()) {
                QProcess comp;
                comp.start("g++", {"-O2", "-std=c++17", cppPath, "-o", binPath});
                comp.waitForFinished(15000);
            }
            if (QFile::exists(binPath)) {
                QFile::setPermissions(binPath, QFile::permissions(binPath) |
                    QFileDevice::ExeOwner | QFileDevice::ExeUser |
                    QFileDevice::ExeGroup | QFileDevice::ExeOther);
                return binPath;
            }
        }

        // 6. .pas source -> compile with fpc if available
        QString pasPath = td.filePath(base + ".pas");
        if (QFile::exists(pasPath)) {
            QString binPath = td.filePath(base + "_linux");
            QFileInfo fiPas(pasPath);
            QFileInfo fiBin(binPath);
            if (!fiBin.exists() || fiBin.lastModified() < fiPas.lastModified()) {
                QProcess comp;
                comp.start("fpc", {"-O2", "-o" + binPath, pasPath});
                comp.waitForFinished(15000);
            }
            if (QFile::exists(binPath)) {
                QFile::setPermissions(binPath, QFile::permissions(binPath) |
                    QFileDevice::ExeOwner | QFileDevice::ExeUser |
                    QFileDevice::ExeGroup | QFileDevice::ExeOther);
                return binPath;
            }
        }
    }

    // Fallback: look for ANY file starting with "check" (case-insensitive) in taskDir
    for (const QString &f : td.entryList(QDir::Files)) {
        if (f.startsWith("check", Qt::CaseInsensitive)) {
            QString full = td.filePath(f);
            if (f.endsWith(".cpp", Qt::CaseInsensitive)) {
                QString binPath = td.filePath(QFileInfo(f).baseName() + "_linux");
                QProcess comp;
                comp.start("g++", {"-O2", "-std=c++17", full, "-o", binPath});
                comp.waitForFinished(15000);
                if (QFile::exists(binPath)) return binPath;
            }
            return full;
        }
    }

    return {};
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
    emit statusChanged(m_cfg.contestantName, taskName, "Dịch bài...");

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
            u.statusText = "Dịch bị lỗi";
            emit updateReady(u);
            emit statusChanged(m_cfg.contestantName, taskName, "Dịch bị lỗi");
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

    QString checkerExe;
    if (checker == CheckerType::C7External || checker == CheckerType::C6AMM2External) {
        checkerExe = findOrBuildChecker(m_cfg.tasksDir + "/" + taskName, taskName, checker);
        if (checkerExe.isEmpty()) {
            emit logMessage(QString("[%1] Cảnh báo: Không tìm thấy trình chấm ngoài cho bài %2 (%3)")
                                .arg(m_cfg.contestantName, taskName, cfg.evaluatorName));
        } else {
            emit logMessage(QString("[%1] Trình chấm ngoài bài %2: %3")
                                .arg(m_cfg.contestantName, taskName, checkerExe));
        }
    }

    DetectedIO detectedIO;
    if (m_cfg.autoFreopen) {
        detectedIO = detectIOFromSource(localSrc);
        if (!detectedIO.input.isEmpty() || !detectedIO.output.isEmpty()) {
            emit logMessage(QString("[%1] Phát hiện freopen trong %2: inp='%3', out='%4'")
                                .arg(m_cfg.contestantName, taskName, detectedIO.input, detectedIO.output));
        }
    }

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
        if (!detectedIO.input.isEmpty()) {
            inpVariations.prepend(detectedIO.input);
        }
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
        if (!detectedIO.output.isEmpty()) {
            QFile::remove(workPath + "/" + detectedIO.output);
        }

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
        emit statusChanged(m_cfg.contestantName, taskName, "Nạp bài làm");

        TestResult tr;
        tr.testName = testName;

        emit statusChanged(m_cfg.contestantName, taskName, "Chạy bài làm");

        if (isInterpreted && foundExt == ".py") {
            QString interpreter = compiler ? compiler->command : "/usr/bin/python3";
            tr = Judger::execute(exeFile, workPath, inpFile, actualOut, cfg.useStdIn, cfg.useStdOut, timeLimit, memLimit, interpreter);
        } else {
            tr = Judger::execute(exeFile, workPath, inpFile, actualOut, cfg.useStdIn, cfg.useStdOut, timeLimit, memLimit, "");
        }
        tr.testName = testName;

        // Search workPath for contestant output file
        QString foundOut;
        QDir wd(workPath);
        QString targetBase = !cfg.outputFile.isEmpty() ? QFileInfo(cfg.outputFile).completeBaseName() : taskName;

        if (!cfg.useStdOut) {
            // 1. Check detected freopen output if non-empty
            if (!detectedIO.output.isEmpty() && QFile::exists(workPath + "/" + detectedIO.output)
                && QFileInfo(workPath + "/" + detectedIO.output).size() > 0) {
                foundOut = workPath + "/" + detectedIO.output;
            } else {
                // 2. Search case-insensitively for defaultOutName, targetBase.out, taskName.out
                for (const QString &f : wd.entryList(QDir::Files)) {
                    if (f.compare(defaultOutName, Qt::CaseInsensitive) == 0 ||
                        f.compare(targetBase + ".out", Qt::CaseInsensitive) == 0 ||
                        f.compare(taskName + ".out", Qt::CaseInsensitive) == 0) {
                        if (QFileInfo(wd.filePath(f)).size() > 0) {
                            foundOut = wd.filePath(f);
                            break;
                        } else if (foundOut.isEmpty()) {
                            foundOut = wd.filePath(f);
                        }
                    }
                }

                // 3. Search for any other .out or .ans file created in workPath
                if (foundOut.isEmpty() || QFileInfo(foundOut).size() == 0) {
                    for (const QString &f : wd.entryList(QDir::Files)) {
                        if (f == taskName ||
                            f.endsWith(".cpp", Qt::CaseInsensitive) || f.endsWith(".c", Qt::CaseInsensitive) ||
                            f.endsWith(".pas", Qt::CaseInsensitive) || f.endsWith(".py", Qt::CaseInsensitive) ||
                            f.endsWith(".inp", Qt::CaseInsensitive) || f.endsWith(".in", Qt::CaseInsensitive) ||
                            f.endsWith(".o", Qt::CaseInsensitive))
                            continue;
                        if (f.endsWith(".out", Qt::CaseInsensitive) || f.endsWith(".ans", Qt::CaseInsensitive)) {
                            foundOut = wd.filePath(f);
                            break;
                        }
                    }
                }
            }

            if (!foundOut.isEmpty()) {
                actualOut = foundOut;
            }
        } else {
            // useStdOut is true: if standard output is empty, but contestant created an output file on disk, use that
            if (!QFile::exists(actualOut) || QFileInfo(actualOut).size() == 0) {
                for (const QString &f : wd.entryList(QDir::Files)) {
                    if (f.compare(defaultOutName, Qt::CaseInsensitive) == 0 ||
                        f.compare(targetBase + ".out", Qt::CaseInsensitive) == 0 ||
                        f.compare(taskName + ".out", Qt::CaseInsensitive) == 0) {
                        if (QFileInfo(wd.filePath(f)).size() > 0) {
                            actualOut = wd.filePath(f);
                            break;
                        }
                    }
                }
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
            emit statusChanged(m_cfg.contestantName, taskName, "Chấm...");
            QString msg;
            double ratio = Judger::checkOutput(outFile, actualOut, checker,
                                               checkerExe, testPath, workPath, msg);
            tr.checkerMsg = msg;
            tr.score      = ratio * testMark;
            if (ratio >= 1.0) {
                tr.verdict = Verdict::AC;
            } else {
                tr.verdict = Verdict::WA;
            }
        } else if (tr.verdict == Verdict::TLE) {
            emit statusChanged(m_cfg.contestantName, taskName, "Kết thúc bài làm");
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Chạy quá thời gian";
        } else if (tr.verdict == Verdict::MLE) {
            emit statusChanged(m_cfg.contestantName, taskName, "Kết thúc bài làm");
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Chạy quá bộ nhớ";
        } else if (tr.verdict == Verdict::RTE) {
            emit statusChanged(m_cfg.contestantName, taskName, "Kết thúc bài làm");
            tr.score = 0.0;
            if (tr.checkerMsg.isEmpty()) tr.checkerMsg = "Chạy sinh lỗi";
        } else {
            emit statusChanged(m_cfg.contestantName, taskName, "Kết thúc bài làm");
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
        u.statusText = "Chấm...";
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
        wcfg.autoFreopen    = m_cfg.autoFreopen;

        auto *w = new JudgeWorker(wcfg, this);
        connect(w,    &JudgeWorker::updateReady,
                this, &JudgeEngine::updateReady);
        connect(w,    &JudgeWorker::statusChanged,
                this, &JudgeEngine::statusChanged);
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
