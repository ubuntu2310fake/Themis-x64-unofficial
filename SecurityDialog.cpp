#include "SecurityDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSysInfo>
#include <QGroupBox>

SecurityDialog::SecurityDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Bảo mật");
    setFixedWidth(440);

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(10);

    // ── Activate checkbox ────────────────────────────────────────────────────
    m_chkActivate = new QCheckBox(
        "Kích hoạt chế độ bảo mật (Xin đọc kỹ hướng dẫn sử dụng trước khi kích hoạt)");
    connect(m_chkActivate, &QCheckBox::toggled, this, &SecurityDialog::onActivateToggled);
    mainLay->addWidget(m_chkActivate);

    // ── Form ─────────────────────────────────────────────────────────────────
    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    m_edtUsername = new QLineEdit();
    m_edtUsername->setEnabled(false);
    form->addRow("Quyền bảo mật:\n(UserName)", m_edtUsername);

    m_edtPassword = new QLineEdit();
    m_edtPassword->setEchoMode(QLineEdit::Password);
    m_edtPassword->setEnabled(false);
    form->addRow("Mật mã:\n(Password)", m_edtPassword);

    m_chkShowPwd = new QCheckBox("Hiện mật mã");
    m_chkShowPwd->setEnabled(false);
    connect(m_chkShowPwd, &QCheckBox::toggled, this, &SecurityDialog::onShowPassword);
    form->addRow("", m_chkShowPwd);

    // Domain (read-only, shows machine hostname)
    m_lblDomain = new QLabel(QSysInfo::machineHostName().toUpper());
    m_lblDomain->setStyleSheet("font-weight: bold;");
    form->addRow("Tên miền:\n(Domain)", m_lblDomain);

    mainLay->addLayout(form);
    mainLay->addStretch();

    // ── Buttons ───────────────────────────────────────────────────────────────
    QHBoxLayout *btnLay = new QHBoxLayout();
    QPushButton *btnHelp = new QPushButton("❓ Hướng dẫn");
    btnLay->addWidget(btnHelp);
    btnLay->addStretch();
    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText("✔ Ghi nhận");
    box->button(QDialogButtonBox::Cancel)->setText("✖ Hủy bỏ");
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    btnLay->addWidget(box);
    mainLay->addLayout(btnLay);
}

void SecurityDialog::onActivateToggled(bool checked)
{
    m_edtUsername->setEnabled(checked);
    m_edtPassword->setEnabled(checked);
    m_chkShowPwd->setEnabled(checked);
}

void SecurityDialog::onShowPassword(bool checked)
{
    m_edtPassword->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}
