#pragma once
#include <QString>
#include <QList>

// ─── Verdict ──────────────────────────────────────────────────────────────────
enum class Verdict {
    Pending,   // Chưa chấm
    Judging,   // Đang chấm
    AC,        // Accepted – Đúng
    WA,        // Wrong Answer – Sai
    TLE,       // Time Limit Exceeded
    MLE,       // Memory Limit Exceeded
    RTE,       // Runtime Error (SIGSEGV / exit != 0)
    CE,        // Compilation Error
    SK,        // Skipped – Không nộp bài
    IE,        // Internal Error
};

inline QString verdictText(Verdict v) {
    switch (v) {
    case Verdict::Pending:  return "";
    case Verdict::Judging:  return "...";
    case Verdict::AC:       return "✓";
    case Verdict::WA:       return "✗";
    case Verdict::TLE:      return "TLE";
    case Verdict::MLE:      return "MLE";
    case Verdict::RTE:      return "RTE";
    case Verdict::CE:       return "CE";
    case Verdict::SK:       return "—";
    case Verdict::IE:       return "IE";
    }
    return "?";
}

inline QString verdictColor(Verdict v) {
    switch (v) {
    case Verdict::AC:      return "#006600";
    case Verdict::WA:      return "#cc0000";
    case Verdict::TLE:     return "#cc6600";
    case Verdict::MLE:     return "#aa00aa";
    case Verdict::RTE:     return "#cc0000";
    case Verdict::CE:      return "#888800";
    case Verdict::SK:      return "#888888";
    case Verdict::IE:      return "#cc0000";
    default:               return "#000000";
    }
}

// ─── One test case result ──────────────────────────────────────────────────────
struct TestResult {
    QString  testName;
    Verdict  verdict    = Verdict::Pending;
    double   timeMs     = 0.0;   // milliseconds
    long     memKb      = 0;     // kilobytes
    double   score      = 0.0;   // 0..1 multiplied by test mark
    QString  checkerMsg;
};

// ─── One task result for one contestant ──────────────────────────────────────
struct TaskResult {
    QString  taskName;
    Verdict  verdict    = Verdict::Pending;
    double   totalScore = 0.0;
    QString  ceLog;              // compilation error output
    QList<TestResult> tests;
};

// ─── Checker type (mirrors Themis DLL names) ─────────────────────────────────
enum class CheckerType {
    C1LinesWordsIgnoreCase,
    C2LinesWordsCase,
    C3WordsIgnoreCase,
    C4WordsCase,
    C5Binary,
    C6AMM2External,
    C7External,
};

inline CheckerType checkerFromName(const QString &name) {
    if (name.contains("C1")) return CheckerType::C1LinesWordsIgnoreCase;
    if (name.contains("C2")) return CheckerType::C2LinesWordsCase;
    if (name.contains("C3")) return CheckerType::C3WordsIgnoreCase;
    if (name.contains("C4")) return CheckerType::C4WordsCase;
    if (name.contains("C5")) return CheckerType::C5Binary;
    if (name.contains("C6")) return CheckerType::C6AMM2External;
    if (name.contains("C7")) return CheckerType::C7External;
    return CheckerType::C1LinesWordsIgnoreCase; // default
}
