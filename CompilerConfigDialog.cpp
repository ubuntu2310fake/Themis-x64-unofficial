#include <QStyle>
#include <QCoreApplication>
#include "CompilerConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QKeySequence>
#include <QShortcut>
#include <QProcess>
#include <QMessageBox>
#include <QStandardPaths>

// ─── Default compiler entries ────────────────────────────────────────────────
QList<CompilerEntry> CompilerConfigDialog::defaultEntries()
{
    QString appDir = QCoreApplication::applicationDirPath();

#ifdef Q_OS_WIN
    auto findWinCompiler = [](const QString &baseDir, const QStringList &subPaths, const QString &exeName) -> QString {
        // 1. Check relative to applicationDirPath (returns native absolute path, NOT %APPDIR%)
        for (const QString &sub : subPaths) {
            QString full = baseDir + "/" + sub;
            if (QFile::exists(full)) {
                return "\"" + QDir::toNativeSeparators(full) + "\"";
            }
        }
        // 2. Check standard system locations on Windows
        QStringList sysDirs = {
            "C:/Program Files (x86)/Themis",
            "C:/Program Files/Themis",
            "C:/Themis",
            "C:/MinGW",
            "C:/msys64/ucrt64",
            "C:/msys64/mingw64",
            "C:/TDM-GCC-64"
        };
        for (const QString &sys : sysDirs) {
            for (const QString &sub : subPaths) {
                QString full = sys + "/" + sub;
                if (QFile::exists(full)) {
                    return "\"" + QDir::toNativeSeparators(full) + "\"";
                }
            }
        }
        // 3. Check system PATH
        QString inPath = QStandardPaths::findExecutable(exeName);
        if (!inPath.isEmpty()) {
            return "\"" + QDir::toNativeSeparators(inPath) + "\"";
        }
        return exeName;
    };

    QString gpp  = findWinCompiler(appDir, {"GCC/bin/g++.exe", "ucrt64/bin/g++.exe", "mingw64/bin/g++.exe", "bin/g++.exe"}, "g++");
    QString gcc  = findWinCompiler(appDir, {"GCC/bin/gcc.exe", "ucrt64/bin/gcc.exe", "mingw64/bin/gcc.exe", "bin/gcc.exe"}, "gcc");
    QString fpc  = findWinCompiler(appDir, {"FPC/bin/i386-win32/fpc.exe", "FPC/bin/x86_64-win64/fpc.exe", "FPC/bin/fpc.exe", "bin/fpc.exe"}, "fpc");
    QString py3  = findWinCompiler(appDir, {"PYTHON/python.exe", "python.exe"}, "python");
    QString java = findWinCompiler(appDir, {"JDK/bin/javac.exe", "bin/javac.exe"}, "javac");

    return {
        { ".cpp",   gpp  + " -O2 -static -x c++ \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\" -Wl,--stack,66060288" },
        { ".c",     gcc  + " -O2 -static -x c \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\" -pipe -lm -Wl,--stack,66060288" },
        { ".pas",   fpc  + " -O2 -XS -Sg \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\"" },
        { ".pp",    fpc  + " -O2 -XS -Sg \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\"" },
        { ".java",  java + " \"%PATH%%NAME%%EXT%\"|@WorkDir=\"%PATH%\"" },
        { ".exe",   ";Nếu không muốn dịch lại khi đã có file .exe, chuyển loại file này lên đầu" },
        { ".class", ";Nếu không muốn dịch lại khi đã có file .class, chuyển loại file này lên đầu" },
        { "",       ";Linux ELF binary – Chạy trực tiếp (không cần biên dịch), chuyển loại file này lên đầu để ưu tiên" },
        { ".py",    py3 },
    };
#else
    auto which = [](const QString &bin) -> QString {
        QString found = QStandardPaths::findExecutable(bin);
        if (!found.isEmpty()) return found;
        QProcess p;
        p.start("which", {bin});
        p.waitForFinished(1000);
        QString out = p.readAllStandardOutput().trimmed();
        return out.isEmpty() ? bin : out;
    };

    QString gpp  = which("g++");
    QString gcc  = which("gcc");
    QString fpc  = which("fpc");
    QString java = which("javac");
    QString py3  = which("python3");

    return {
        { ".cpp",   gpp  + " -O2 -x c++ \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\"" },
        { ".c",     gcc  + " -O2 -x c \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\" -pipe -lm" },
        { ".pas",   fpc  + " -O2 -XS -Sg \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\"" },
        { ".pp",    fpc  + " -O2 -XS -Sg \"%PATH%%NAME%%EXT%\" -o \"%PATH%%NAME%\"" },
        { ".java",  java + " \"%PATH%%NAME%%EXT%\"|@WorkDir=\"%PATH%\"" },
        { ".exe",   ";Nếu không muốn dịch lại khi đã có file .exe, chuyển loại file này lên đầu" },
        { ".class", ";Nếu không muốn dịch lại khi đã có file .class, chuyển loại file này lên đầu" },
        { "",       ";Linux ELF binary – Chạy trực tiếp (không cần biên dịch), chuyển loại file này lên đầu để ưu tiên" },
        { ".py",    py3 },
    };
#endif
}

// ─── Auto-relocate & self-heal compiler commands ─────────────────────────────
QString CompilerConfigDialog::autoRelocateCommand(const QString &rawCmd, const QString &ext)
{
    QString cmd = rawCmd.trimmed();
    if (cmd.startsWith(';')) {
        return rawCmd; // Keep comments as-is
    }

    QString appDir = QCoreApplication::applicationDirPath();

    // 1. Expand %APPDIR% if present
    if (cmd.contains("%APPDIR%", Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        cmd.replace("%APPDIR%", QDir::toNativeSeparators(appDir), Qt::CaseInsensitive);
#else
        cmd.replace("%APPDIR%", appDir, Qt::CaseInsensitive);
#endif
    }

#ifdef Q_OS_WIN
    // 2. Parse executable and argument parts
    QString exePart;
    QString rest;
    bool isQuoted = false;

    if (cmd.startsWith('"')) {
        int endQ = cmd.indexOf('"', 1);
        if (endQ != -1) {
            exePart = cmd.mid(1, endQ - 1);
            rest = cmd.mid(endQ + 1);
            isQuoted = true;
        } else {
            exePart = cmd.mid(1);
            rest = "";
        }
    } else {
        int spaceIdx = cmd.indexOf(' ');
        if (spaceIdx != -1) {
            exePart = cmd.left(spaceIdx);
            rest = cmd.mid(spaceIdx);
        } else {
            exePart = cmd;
            rest = "";
        }
    }

    // Candidate relative paths for each compiler/toolchain
    QStringList candidates;
    if (ext.compare(".cpp", Qt::CaseInsensitive) == 0 || exePart.contains("g++", Qt::CaseInsensitive)) {
        candidates = {"GCC/bin/g++.exe", "ucrt64/bin/g++.exe", "mingw64/bin/g++.exe", "bin/g++.exe"};
    } else if (ext.compare(".c", Qt::CaseInsensitive) == 0 || exePart.contains("gcc", Qt::CaseInsensitive)) {
        candidates = {"GCC/bin/gcc.exe", "ucrt64/bin/gcc.exe", "mingw64/bin/gcc.exe", "bin/gcc.exe"};
    } else if (ext.compare(".pas", Qt::CaseInsensitive) == 0 || ext.compare(".pp", Qt::CaseInsensitive) || exePart.contains("fpc", Qt::CaseInsensitive)) {
        candidates = {"FPC/bin/i386-win32/fpc.exe", "FPC/bin/x86_64-win64/fpc.exe", "FPC/bin/fpc.exe", "bin/fpc.exe"};
    } else if (ext.compare(".py", Qt::CaseInsensitive) == 0 || exePart.contains("python", Qt::CaseInsensitive)) {
        candidates = {"PYTHON/python.exe", "python.exe"};
    } else if (ext.compare(".java", Qt::CaseInsensitive) == 0 || exePart.contains("javac", Qt::CaseInsensitive)) {
        candidates = {"JDK/bin/javac.exe", "bin/javac.exe"};
    }

    bool exists = QFile::exists(exePart);

    // If it points to an old nonexistent path or contains ucrt64 that no longer exists
    if (!exists || (exePart.contains("ucrt64", Qt::CaseInsensitive) && !QFile::exists(exePart))) {
        QString foundNewExe;

        // Try to match relative directory inside appDir (e.g. /GCC/, /FPC/, /JDK/, /PYTHON/)
        QString normExe = exePart;
        normExe.replace('\\', '/');
        for (const QString &prefix : {"/GCC/", "/FPC/", "/JDK/", "/PYTHON/", "/ucrt64/", "/mingw64/"}) {
            int idx = normExe.indexOf(prefix, 0, Qt::CaseInsensitive);
            if (idx != -1) {
                QString rel = normExe.mid(idx + 1);
                // Map ucrt64 to GCC if ucrt64 doesn't exist
                if (rel.startsWith("ucrt64/", Qt::CaseInsensitive) && !QFile::exists(appDir + "/" + rel)) {
                    rel.replace(0, 6, "GCC");
                }
                if (QFile::exists(appDir + "/" + rel)) {
                    foundNewExe = QDir::toNativeSeparators(appDir + "/" + rel);
                    break;
                }
            }
        }

        // Try candidate subpaths in appDir
        if (foundNewExe.isEmpty()) {
            for (const QString &sub : candidates) {
                if (QFile::exists(appDir + "/" + sub)) {
                    foundNewExe = QDir::toNativeSeparators(appDir + "/" + sub);
                    break;
                }
            }
        }

        // Try standard system locations
        if (foundNewExe.isEmpty()) {
            QStringList sysDirs = {
                "C:/Program Files (x86)/Themis",
                "C:/Program Files/Themis",
                "C:/Themis",
                "C:/MinGW",
                "C:/msys64/ucrt64",
                "C:/msys64/mingw64",
                "C:/TDM-GCC-64"
            };
            for (const QString &sys : sysDirs) {
                for (const QString &sub : candidates) {
                    QString full = sys + "/" + sub;
                    if (QFile::exists(full)) {
                        foundNewExe = QDir::toNativeSeparators(full);
                        break;
                    }
                }
                if (!foundNewExe.isEmpty()) break;
            }
        }

        // Try system PATH
        if (foundNewExe.isEmpty()) {
            QString exeName = QFileInfo(exePart).fileName();
            if (exeName.isEmpty() && !candidates.isEmpty()) exeName = QFileInfo(candidates.first()).fileName();
            if (!exeName.isEmpty()) {
                QString inPath = QStandardPaths::findExecutable(exeName);
                if (!inPath.isEmpty()) {
                    foundNewExe = QDir::toNativeSeparators(inPath);
                }
            }
        }

        if (!foundNewExe.isEmpty()) {
            exePart = foundNewExe;
            isQuoted = true;
        }
    }

    // Ensure C++ / C GCC on Windows includes -static flag so student .exe don't require external DLLs
    if (ext.compare(".cpp", Qt::CaseInsensitive) == 0 && exePart.contains("g++", Qt::CaseInsensitive) && !rest.contains("-static")) {
        rest = " -static" + rest;
    }
    if (ext.compare(".c", Qt::CaseInsensitive) == 0 && exePart.contains("gcc", Qt::CaseInsensitive) && !rest.contains("-static")) {
        rest = " -static" + rest;
    }

    if (isQuoted || exePart.contains(' ')) {
        return "\"" + QDir::toNativeSeparators(exePart) + "\"" + rest;
    } else {
        return QDir::toNativeSeparators(exePart) + rest;
    }
#else
    Q_UNUSED(ext);
    return cmd;
#endif
}

bool CompilerConfigDialog::autoRelocateEntries(QList<CompilerEntry> &entries)
{
    bool changed = false;
    for (CompilerEntry &e : entries) {
        QString updated = autoRelocateCommand(e.command, e.ext);
        if (updated != e.command) {
            e.command = updated;
            changed = true;
        }
    }
    return changed;
}

// ─── Constructor ─────────────────────────────────────────────────────────────
CompilerConfigDialog::CompilerConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Cấu hình bộ dịch");
    setMinimumSize(740, 480);
    m_entries = defaultEntries();
    buildUI();
}

void CompilerConfigDialog::buildUI()
{
    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 8);
    mainLay->setSpacing(6);

    // ── Table ────────────────────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({"Loại file", "Lệnh dịch"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 75);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->verticalHeader()->setVisible(true);  // show row numbers like original
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::currentCellChanged,
            this, &CompilerConfigDialog::onCurrentCellChanged);
    mainLay->addWidget(m_table);

    // ── Browse row (changes label based on selected row) ──────────────────────
    m_browseWidget = new QWidget(this);
    QHBoxLayout *browseLay = new QHBoxLayout(m_browseWidget);
    browseLay->setContentsMargins(4, 2, 4, 2);

    m_labelBrowseHint = new QLabel("Chọn dòng để cấu hình đường dẫn chương trình dịch:");
    m_labelBrowseHint->setStyleSheet("color: #333; font-size: 11px;");
    browseLay->addWidget(m_labelBrowseHint, 1);

    m_btnBrowseExe = new QPushButton("🔍  Chọn trình thông dịch Python...");
    m_btnBrowseExe->setEnabled(false);
    m_btnBrowseExe->setFixedHeight(28);
    connect(m_btnBrowseExe, &QPushButton::clicked, this, &CompilerConfigDialog::onBrowseExe);
    browseLay->addWidget(m_btnBrowseExe);
    mainLay->addWidget(m_browseWidget);

    // ── Hints ────────────────────────────────────────────────────────────────
    QGroupBox *hintBox = new QGroupBox();
    QVBoxLayout *hintLay = new QVBoxLayout(hintBox);
    hintLay->setContentsMargins(6, 4, 6, 4);
    hintLay->setSpacing(2);
    for (const QString &h : {
            QString("<font color='#2255cc'>●</font> Thêm: Bấm ↓ tại mục cuối"),
            QString("<font color='#2255cc'>●</font> Chèn: Insert;  Xóa: CTRL + Delete"),
            QString("<font color='#2255cc'>●</font> Chuyển lên: CTRL + U;  Chuyển xuống: CTRL + D")}) {
        auto *l = new QLabel(h);
        l->setTextFormat(Qt::RichText);
        l->setStyleSheet("font-size:11px;");
        hintLay->addWidget(l);
    }
    mainLay->addWidget(hintBox);

    // ── Buttons ───────────────────────────────────────────────────────────────
    QHBoxLayout *btnLay = new QHBoxLayout();
    QPushButton *btnHelp = new QPushButton("❓ Hướng dẫn");
    btnLay->addWidget(btnHelp);

    QPushButton *btnReset = new QPushButton("🔄 Khôi phục mặc định");
    btnLay->addWidget(btnReset);
    connect(btnReset, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Khôi phục mặc định",
            "Bạn có chắc chắn muốn khôi phục lại danh sách bộ dịch mặc định theo vị trí hiện tại của Themis?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_entries = defaultEntries();
            refreshTable();
        }
    });

    btnLay->addStretch();
    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *btnOk = box->button(QDialogButtonBox::Ok);
    QPushButton *btnCancel = box->button(QDialogButtonBox::Cancel);
    btnOk->setText(" Ghi nhận");
    btnOk->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    btnCancel->setText(" Hủy bỏ");
    btnCancel->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    connect(box, &QDialogButtonBox::accepted, this, &CompilerConfigDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    btnLay->addWidget(box);
    mainLay->addLayout(btnLay);

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    new QShortcut(QKeySequence("Ctrl+U"),      this, this, &CompilerConfigDialog::onMoveUp);
    new QShortcut(QKeySequence("Ctrl+D"),      this, this, &CompilerConfigDialog::onMoveDown);
    new QShortcut(QKeySequence("Ctrl+Delete"), this, this, &CompilerConfigDialog::onDeleteRow);
    new QShortcut(QKeySequence("Insert"),      this, this, &CompilerConfigDialog::onInsertRow);

    refreshTable();
}

// ─── Table population ─────────────────────────────────────────────────────────
void CompilerConfigDialog::refreshTable()
{
    m_table->setRowCount(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        const CompilerEntry &e = m_entries[i];

        // Display "(elf)" for empty extension (Linux ELF binary)
        QString displayExt = e.ext.isEmpty() ? "(elf)" : e.ext;

        auto *extItem = new QTableWidgetItem(displayExt);
        extItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

        auto *cmdItem = new QTableWidgetItem(e.command);

        m_table->setItem(i, 0, extItem);
        m_table->setItem(i, 1, cmdItem);

        // Gray out comment rows (including ELF note)
        if (e.command.startsWith(';')) {
            extItem->setForeground(QColor(120,120,120));
            cmdItem->setForeground(QColor(120,120,120));
            extItem->setBackground(QColor(240,240,240));
            cmdItem->setBackground(QColor(240,240,240));
        }
        // Highlight python row
        if (e.ext == ".py") {
            extItem->setBackground(QColor(255, 248, 220));
            cmdItem->setBackground(QColor(255, 248, 220));
        }
        // Highlight ELF row with a light blue tint
        if (e.ext.isEmpty()) {
            extItem->setBackground(QColor(220, 235, 255));
            cmdItem->setBackground(QColor(220, 235, 255));
        }
    }
}

// ─── Row type helpers ──────────────────────────────────────────────────────
bool CompilerConfigDialog::isCompilerRow(int row) const
{
    if (row < 0 || row >= m_entries.size()) return false;
    return !m_entries[row].command.startsWith(';');
}

bool CompilerConfigDialog::isPythonRow(int row) const
{
    if (row < 0 || row >= m_entries.size()) return false;
    return m_entries[row].ext == ".py";
}

QString CompilerConfigDialog::browseLabelFor(int row) const
{
    if (row < 0 || row >= m_entries.size()) return "";
    const QString &ext = m_entries[row].ext;
    if (ext == ".py")   return "🔍  Chọn trình thông dịch Python...";
    if (ext == ".cpp" || ext == ".c")  return "🔍  Chọn trình biên dịch C/C++...";
    if (ext == ".pas" || ext == ".pp") return "🔍  Chọn trình biên dịch Pascal (fpc)...";
    if (ext == ".java")                return "🔍  Chọn trình biên dịch Java (javac)...";
    return "🔍  Chọn chương trình thực thi...";
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void CompilerConfigDialog::onCurrentCellChanged(int currentRow, int, int, int)
{
    bool canBrowse = isCompilerRow(currentRow);
    m_btnBrowseExe->setEnabled(canBrowse);

    if (canBrowse) {
        QString label = browseLabelFor(currentRow);
        m_btnBrowseExe->setText(label);

        if (isPythonRow(currentRow)) {
            m_labelBrowseHint->setText(
                "Dòng <b>.py</b>: Nhập đường dẫn trình thông dịch Python "
                "(ví dụ: <code>/usr/bin/python3</code>)");
        } else {
            QString ext = m_entries[currentRow].ext;
            m_labelBrowseHint->setText(
                "Dòng <b>" + ext + "</b>: Có thể nhập thẳng tên lệnh "
                "hoặc đường dẫn đầy đủ tới chương trình dịch.");
        }
    } else {
        m_btnBrowseExe->setText("🔍  Chọn chương trình dịch...");
        m_labelBrowseHint->setText("Chọn một dòng trình dịch để cấu hình đường dẫn:");
    }
}

void CompilerConfigDialog::onBrowseExe()
{
    int row = m_table->currentRow();
    if (row < 0 || !isCompilerRow(row)) return;

    QString startDir;
    QString title = "Chọn chương trình dịch";

#ifdef Q_OS_WIN
    startDir = QCoreApplication::applicationDirPath();
    QString filter = "Chương trình thực thi (*.exe);;Tất cả các file (*.*)";
#else
    startDir = "/usr/bin/";
    QString filter = "Executables (*);;All files (*)";
#endif

    // For Python, look in common places
    if (isPythonRow(row)) {
        title = "Chọn trình thông dịch Python";
#ifdef Q_OS_WIN
        if (QFile::exists(startDir + "/PYTHON/python.exe")) {
            startDir = startDir + "/PYTHON";
        }
#endif
    }

    QString path = QFileDialog::getOpenFileName(this, title, startDir, filter);
    if (path.isEmpty()) return;

#ifdef Q_OS_WIN
    path = QDir::toNativeSeparators(path);
#endif
    QString quotePath = path.contains(' ') ? ("\"" + path + "\"") : path;

    // For Python: just store the interpreter path
    // For others: replace the executable in the existing command line
    QString currentCmd = m_table->item(row, 1)->text();
    if (isPythonRow(row)) {
        m_table->item(row, 1)->setText(quotePath);
        m_entries[row].command = quotePath;
    } else {
        QString rest;
        if (currentCmd.trimmed().startsWith('"')) {
            int endQ = currentCmd.indexOf('"', currentCmd.indexOf('"') + 1);
            if (endQ != -1) {
                rest = currentCmd.mid(endQ + 1);
            }
        } else {
            int spaceIdx = currentCmd.indexOf(' ');
            if (spaceIdx != -1) {
                rest = currentCmd.mid(spaceIdx);
            }
        }
        QString newCmd = quotePath + rest;
        m_table->item(row, 1)->setText(newCmd);
        m_entries[row].command = newCmd;
    }
}

void CompilerConfigDialog::onInsertRow()
{
    int r = m_table->currentRow();
    if (r < 0) r = 0;
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(""));
    m_table->setItem(r, 1, new QTableWidgetItem(""));
    m_table->setCurrentCell(r, 0);
    m_table->editItem(m_table->item(r, 0));
}

void CompilerConfigDialog::onDeleteRow()
{
    int r = m_table->currentRow();
    if (r >= 0) { m_table->removeRow(r); m_entries.removeAt(r); }
}

void CompilerConfigDialog::onMoveUp()
{
    int r = m_table->currentRow();
    if (r <= 0) return;
    for (int c = 0; c < 2; ++c) {
        auto *a = m_table->takeItem(r, c);
        auto *b = m_table->takeItem(r-1, c);
        m_table->setItem(r-1, c, a);
        m_table->setItem(r,   c, b);
    }
    m_entries.swapItemsAt(r, r-1);
    m_table->setCurrentCell(r-1, m_table->currentColumn());
}

void CompilerConfigDialog::onMoveDown()
{
    int r = m_table->currentRow();
    if (r < 0 || r >= m_table->rowCount()-1) return;
    for (int c = 0; c < 2; ++c) {
        auto *a = m_table->takeItem(r, c);
        auto *b = m_table->takeItem(r+1, c);
        m_table->setItem(r+1, c, a);
        m_table->setItem(r,   c, b);
    }
    m_entries.swapItemsAt(r, r+1);
    m_table->setCurrentCell(r+1, m_table->currentColumn());
}

void CompilerConfigDialog::accept()
{
    m_entries.clear();
    for (int i = 0; i < m_table->rowCount(); ++i) {
        QString ext = m_table->item(i, 0) ? m_table->item(i, 0)->text().trimmed() : "";
        QString cmd = m_table->item(i, 1) ? m_table->item(i, 1)->text().trimmed() : "";
        if (!ext.isEmpty())
            m_entries.append({ext, cmd});
    }
    QDialog::accept();
}

QList<CompilerEntry> CompilerConfigDialog::entries() const { return m_entries; }
void CompilerConfigDialog::setEntries(const QList<CompilerEntry> &e)
{
    m_entries = e;
    autoRelocateEntries(m_entries);
    refreshTable();
}
