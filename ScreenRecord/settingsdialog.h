#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;

// モニター / マイク / フレームレートの設定ダイアログ
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // 現在の設定を QSettings から読み込んで UI に反映
    void load();
    // UI の内容を QSettings に保存
    void save();

    static QString settingsFilePath();

private:
    void refreshAudioDevices(const QString &select);

    QComboBox *m_screenCombo = nullptr;
    QComboBox *m_micCombo    = nullptr;
    QComboBox *m_fpsCombo    = nullptr;
};

#endif // SETTINGSDIALOG_H
