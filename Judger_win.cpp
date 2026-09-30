#include "Judger.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>

TestResult Judger::execute(const QString &exeFile,
                           const QString &workDir,
                           const QString &inputFile,
                           const QString &actualOutputFile,
                           bool useStdIn,
                           bool useStdOut,
                           double timeLimitSec,
                           int    memLimitMb,
                           const QString &interpreter)
{
    TestResult res;
    res.verdict = Verdict::RTE;
    res.timeMs = 0;
    res.memKb = 0;
    
    QString cmdLine;
    if (!interpreter.isEmpty()) {
        cmdLine = "\"" + interpreter + "\" \"" + exeFile + "\"";
    } else {
        cmdLine = "\"" + exeFile + "\"";
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hIn = INVALID_HANDLE_VALUE;
    HANDLE hOut = INVALID_HANDLE_VALUE;
    
    if (useStdIn) {
        hIn = CreateFileW((const wchar_t*)inputFile.utf16(), GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (useStdOut) {
        hOut = CreateFileW((const wchar_t*)actualOutputFile.utf16(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = useStdIn ? hIn : INVALID_HANDLE_VALUE;
    si.hStdOutput = useStdOut ? hOut : INVALID_HANDLE_VALUE;
    si.hStdError = INVALID_HANDLE_VALUE; // or a nul handle

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring wCmd = cmdLine.toStdWString();
    std::wstring wDir = workDir.toStdWString();

    QElapsedTimer wallTimer;
    wallTimer.start();

    if (!CreateProcessW(NULL, &wCmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, wDir.c_str(), &si, &pi)) {
        if (hIn != INVALID_HANDLE_VALUE) CloseHandle(hIn);
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        res.verdict = Verdict::IE;
        res.checkerMsg = "CreateProcess failed";
        return res;
    }
    
    // Memory limit via Job Object
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
        ZeroMemory(&jeli, sizeof(jeli));
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        jeli.ProcessMemoryLimit = (SIZE_T)memLimitMb * 1024 * 1024;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        AssignProcessToJobObject(hJob, pi.hProcess);
    }

    ResumeThread(pi.hThread);

    int timeoutMs = (int)(timeLimitSec * 1000.0);
    DWORD waitRes = WaitForSingleObject(pi.hProcess, timeoutMs + 1000); // 1s grace for wall clock

    // Get Memory (Peak Working Set)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
        res.memKb = pmc.PeakWorkingSetSize / 1024;
    }

    // Get CPU time
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(pi.hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER uKernel, uUser;
        uKernel.LowPart = ftKernel.dwLowDateTime;
        uKernel.HighPart = ftKernel.dwHighDateTime;
        uUser.LowPart = ftUser.dwLowDateTime;
        uUser.HighPart = ftUser.dwHighDateTime;
        // 100-nanosecond intervals
        res.timeMs = (uKernel.QuadPart + uUser.QuadPart) / 10000.0;
    } else {
        res.timeMs = wallTimer.elapsed();
    }

    if (waitRes == WAIT_TIMEOUT || res.timeMs >= timeLimitSec * 1000.0) {
        TerminateProcess(pi.hProcess, 0);
        res.verdict = Verdict::TLE;
        res.timeMs = timeLimitSec * 1000.0;
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        if (exitCode != 0) {
            // Might be MLE killed by JobObject, or RTE
            if (res.memKb >= memLimitMb * 1024 * 0.95) {
                res.verdict = Verdict::MLE;
            } else {
                res.verdict = Verdict::RTE;
            }
        } else {
            res.verdict = Verdict::Judging;
        }
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (hJob) CloseHandle(hJob);
    if (hIn != INVALID_HANDLE_VALUE) CloseHandle(hIn);
    if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);

    return res;
}
#endif
