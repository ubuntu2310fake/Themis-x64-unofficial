#ifndef SECURITYDIALOG_H
#define SECURITYDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

class SecurityDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SecurityDialog(QWidget *parent = nullptr);
    ~SecurityDialog() {}

private slots:
    void onActivateToggled(bool checked);
    void onShowPassword(bool checked);

private:
    QCheckBox *m_chkActivate;
    QLineEdit *m_edtUsername;
    QLineEdit *m_edtPassword;
    QCheckBox *m_chkShowPwd;
    QLabel    *m_lblDomain;
};

#endif // SECURITYDIALOG_H
