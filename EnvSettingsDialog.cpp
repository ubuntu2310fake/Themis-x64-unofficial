#include "EnvSettingsDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QProcess>
#include <QTabWidget>
#include <QDir>
// ─── Helper: auto-detect a binary ─────────────────────────────────────────────
static QString whichBin(const QString &bin)
{
    QProcess p;
    p.start("which", {bin});
    p.waitForFinished(1500);
    QString out = p.readAllStandardOutput().trimmed();
    return out.isEmpty() ? bin : out;
}

// ─── Helper: create a path field with browse+detect buttons ───────────────────
static QHBoxLayout* makeExeRow(QLineEdit *&edt, const QString &defaultVal,
                                const QString &browseTitle, QWidget *parent,
                                bool isDir = false)
{
    QHBoxLayout *lay = new QHBoxLayout();
    edt = new QLineEdit(defaultVal, parent);

    QPushButton *btnBrowse = new QPushButton("🔍");
    btnBrowse->setFixedWidth(30);
    btnBrowse->setToolTip("Chọn " + browseTitle);
    QObject::connect(btnBrowse, &QPushButton::clicked, [edt, browseTitle, isDir, parent](){
        QString result;
        if (isDir)
            result = QFileDialog::getExistingDirectory(parent, "Chọn " + browseTitle, edt->text());
        else
            result = QFileDialog::getOpenFileName(parent, "Chọn " + browseTitle,
                                                   "/usr/bin/", "Executables (*)");
        if (!result.isEmpty()) edt->setText(result);
    });

    lay->addWidget(edt, 1);
    lay->addWidget(btnBrowse);
    return lay;
}

// ─── Constructor ──────────────────────────────────────────────────────────────
EnvSettingsDialog::EnvSettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Thiết lập môi trường");
    setMinimumWidth(560);

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(8);

    QTabWidget *tabs = new QTabWidget(this);

    // ══════════════════════════════════════════════════════════════════════════
    // Tab 1: Cài đặt chung (giống Themis gốc)
    // ══════════════════════════════════════════════════════════════════════════
    QWidget *tabGeneral = new QWidget();
    QFormLayout *formGen = new QFormLayout(tabGeneral);
    formGen->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formGen->setHorizontalSpacing(12);
    formGen->setVerticalSpacing(10);
    formGen->setContentsMargins(12, 12, 12, 12);

    formGen->addRow("Thư mục nộp bài trực tuyến:",
                    makeExeRow(m_edtOnlineDir, "", "thư mục nộp bài", tabGeneral, true));

    QString defaultTemp = QDir::tempPath() + "/Themis/";

    formGen->addRow("Thư mục chứa kỳ thi giải nén:",
                    makeExeRow(m_edtExtractDir, defaultTemp, "thư mục giải nén", tabGeneral, true));

    formGen->addRow("Thư mục chứa \"phòng thi\":",
                    makeExeRow(m_edtExamRoomDir, defaultTemp, "thư mục phòng thi", tabGeneral, true));

    // Number format
    QHBoxLayout *numLay = new QHBoxLayout();
    m_edtNumFormat = new QLineEdit("%.2f");
    m_edtNumFormat->setFixedWidth(80);
    numLay->addWidget(m_edtNumFormat);
    numLay->addWidget(new QLabel("   Ví dụ: π = 3.14"));
    numLay->addStretch();
    formGen->addRow("Khuôn dạng số thập phân:", numLay);

    // Toolbar toggle
    m_chkShowToolbar = new QCheckBox("Hiện thanh công cụ");
    m_chkShowToolbar->setChecked(true);
    formGen->addRow("", m_chkShowToolbar);

    tabs->addTab(tabGeneral, "Cài đặt chung");

    // ══════════════════════════════════════════════════════════════════════════
    // Tab 2: Linux – Ứng dụng hệ thống
    // ══════════════════════════════════════════════════════════════════════════
    QWidget *tabLinux = new QWidget();
    QFormLayout *formLin = new QFormLayout(tabLinux);
    formLin->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLin->setHorizontalSpacing(12);
    formLin->setVerticalSpacing(12);
    formLin->setContentsMargins(12, 12, 12, 12);

    // Auto-detect defaults
    QString detectedTerminal    = whichBin("gnome-terminal");
    if (detectedTerminal == "gnome-terminal") detectedTerminal = whichBin("konsole");
    if (detectedTerminal == "konsole")        detectedTerminal = whichBin("xterm");

    QString detectedFileMgr = whichBin("nautilus");
    if (detectedFileMgr == "nautilus") detectedFileMgr = whichBin("dolphin");
    if (detectedFileMgr == "dolphin")  detectedFileMgr = whichBin("thunar");
    if (detectedFileMgr == "thunar")   detectedFileMgr = whichBin("nemo");

    QString detectedEditor = whichBin("gedit");
    if (detectedEditor == "gedit")       detectedEditor = whichBin("kate");
    if (detectedEditor == "kate")        detectedEditor = whichBin("mousepad");
    if (detectedEditor == "mousepad")    detectedEditor = whichBin("xed");
    if (detectedEditor == "xed")         detectedEditor = whichBin("nano");

    formLin->addRow("Terminal emulator:",
                    makeExeRow(m_edtTerminal, detectedTerminal,
                               "terminal emulator", tabLinux));
    auto *termHint = new QLabel(
        "<i style='color:gray; font-size:10px;'>"
        "Dùng để mở cửa sổ terminal khi cần.<br/>"
        "Ví dụ: gnome-terminal, konsole, xfce4-terminal, xterm</i>");
    termHint->setTextFormat(Qt::RichText);
    formLin->addRow("", termHint);

    formLin->addRow("File manager:",
                    makeExeRow(m_edtFileManager, detectedFileMgr,
                               "file manager", tabLinux));
    auto *fmHint = new QLabel(
        "<i style='color:gray; font-size:10px;'>"
        "Dùng để mở thư mục trong giao diện đồ họa.<br/>"
        "Ví dụ: nautilus, dolphin, thunar, nemo, pcmanfm</i>");
    fmHint->setTextFormat(Qt::RichText);
    formLin->addRow("", fmHint);

    formLin->addRow("Text editor:",
                    makeExeRow(m_edtTextEditor, detectedEditor,
                               "text editor", tabLinux));
    auto *edHint = new QLabel(
        "<i style='color:gray; font-size:10px;'>"
        "Dùng để mở file log, source code của thí sinh.<br/>"
        "Ví dụ: gedit, kate, mousepad, pluma, nano, vim</i>");
    edHint->setTextFormat(Qt::RichText);
    formLin->addRow("", edHint);

    tabs->addTab(tabLinux, "Linux – Ứng dụng");

    mainLay->addWidget(tabs);

    // ── Buttons ───────────────────────────────────────────────────────────────
    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText("✔ Ghi nhận");
    box->button(QDialogButtonBox::Cancel)->setText("✖ Hủy bỏ");
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLay->addWidget(box);
}
