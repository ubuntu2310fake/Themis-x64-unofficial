#ifndef ENVSETTINGSDIALOG_H
#define ENVSETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

class EnvSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EnvSettingsDialog(QWidget *parent = nullptr);
    ~EnvSettingsDialog() {}

private:
    // ── Original Themis fields ──
    QLineEdit *m_edtOnlineDir;
    QLineEdit *m_edtExtractDir;
    QLineEdit *m_edtExamRoomDir;
    QLineEdit *m_edtNumFormat;
    QCheckBox *m_chkShowToolbar;

    // ── Linux-specific ──
    QLineEdit *m_edtTerminal;      // e.g. gnome-terminal, konsole, xterm
    QLineEdit *m_edtFileManager;   // e.g. nautilus, dolphin, thunar, nemo
    QLineEdit *m_edtTextEditor;    // e.g. gedit, kate, mousepad, nano
};

#endif // ENVSETTINGSDIALOG_H
