#pragma once
#include "JudgeTypes.h"
#include "CompilerConfigDialog.h"
#include <QString>
#include <QStringList>

// ─── Judger – low-level compile / run / check ─────────────────────────────────
class Judger
{
public:
    // ── Compile ──────────────────────────────────────────────────────────────
    // Returns true on success.  ceLog gets compiler stderr on failure.
    static bool compile(const QString &sourceFile,  // /tmp/.../TASKNAME.cpp
                        const QString &outputExe,   // /tmp/.../TASKNAME
                        const CompilerEntry &entry, // from CompilerConfigDialog
                        const QString &taskName,    // "LIQ"
                        QString &ceLog);

    // ── Execute one test ──────────────────────────────────────────────────────
    // Returns TestResult with verdict TLE/MLE/RTE or Judging (needs check)
    // actualOutputFile receives the program's stdout.
    static TestResult execute(const QString &exeFile,
                              const QString &workDir,
                              const QString &inputFile,
                              const QString &actualOutputFile,
                              bool useStdIn,
                              bool useStdOut,
                              double timeLimitSec,
                              int    memLimitMb,
                              const QString &interpreter = "");

    // ── Check output ──────────────────────────────────────────────────────────
    // Returns verdict AC/WA and score in [0,1].
    static double checkOutput(const QString &expectedFile,
                              const QString &actualFile,
                              CheckerType   type,
                              const QString &checkerExe,   // for C7
                              const QString &testDir,      // for C7
                              const QString &workDir,      // for C7
                              QString &msg);

private:
    // Checker internals
    static double checkLinesWords(const QString &expected, const QString &actual,
                                  bool caseSensitive);
    static double checkWords(const QString &expected, const QString &actual,
                             bool caseSensitive);
    static double checkBinary(const QString &expected, const QString &actual);
};
