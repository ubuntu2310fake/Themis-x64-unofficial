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

// ─── Default compiler entries for Linux ──────────────────────────────────────
QList<CompilerEntry> CompilerConfigDialog::defaultEntries()
{
    // Auto-detect common compiler paths
    auto which = [](const QString &bin) -> QString {
        QProcess p;
        p.start("which", {bin});
        p.waitForFinished(2000);
        QString out = p.readAllStandardOutput().trimmed();
        return out.isEmpty() ? bin : out;
    };

    QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    QString gpp  = "\"%APPDIR%/ucrt64/bin/g++.exe\"";
    QString gcc  = "\"%APPDIR%/ucrt64/bin/gcc.exe\"";
    QString fpc  = "fpc";
    QString java = "javac";
    QString py3  = "\"%APPDIR%/PYTHON/python.exe\"";
#else
    QString gpp  = "\"%APPDIR%/ucrt64/bin/g++.exe\"";
    if (!QFile::exists(appDir + "/ucrt64/bin/g++.exe")) gpp = which("g++");
    
    QString gcc  = "\"%APPDIR%/ucrt64/bin/gcc.exe\"";
    if (!QFile::exists(appDir + "/ucrt64/bin/gcc.exe")) gcc = which("gcc");
    
    QString fpc  = which("fpc");
    QString java = which("javac");
    
    QString py3  = "\"%APPDIR%/PYTHON/python.exe\"";
    if (!QFile::exists(appDir + "/PYTHON/python.exe")) py3 = which("python3");
#endif

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

    QString startDir = "/usr/bin/";
    QString title    = "Chọn chương trình dịch";

    // For Python, look in common places
    if (isPythonRow(row)) {
        title = "Chọn trình thông dịch Python";
        startDir = "/usr/bin/";
    }

    QString path = QFileDialog::getOpenFileName(
        this, title, startDir,
        "Executables (*);;All files (*)");
    if (path.isEmpty()) return;

    // For Python: just store the interpreter path
    // For others: replace the executable in the existing command line
    QString currentCmd = m_table->item(row, 1)->text();
    if (isPythonRow(row)) {
        // Python: just the interpreter path, we wrap it at judge time
        m_table->item(row, 1)->setText(path);
        m_entries[row].command = path;
    } else {
        // For other compilers: replace the first token (the exe) in the command
        // with the new selected path
        QStringList parts = currentCmd.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            parts[0] = path;
            QString newCmd = parts.join(' ');
            m_table->item(row, 1)->setText(newCmd);
            m_entries[row].command = newCmd;
        }
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
void CompilerConfigDialog::setEntries(const QList<CompilerEntry> &e) { m_entries = e; refreshTable(); }
