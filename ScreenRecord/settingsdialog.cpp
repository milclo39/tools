#include "settingsdialog.h"
#include "recorder.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSettings>

QString SettingsDialog::settingsFilePath()
{
    // アプリと同じフォルダに ini で保存 (ポータブル)
    return QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini");
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("設定"));
    setMinimumWidth(380);

    m_screenCombo = new QComboBox(this);
    m_micCombo    = new QComboBox(this);
    m_fpsCombo    = new QComboBox(this);

    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        const QSize px = s->size() * s->devicePixelRatio();
        m_screenCombo->addItem(QStringLiteral("%1 (%2x%3)")
                                   .arg(s->name()).arg(px.width()).arg(px.height()),
                               s->name());
    }

    m_fpsCombo->addItem(QStringLiteral("15 fps"), 15);
    m_fpsCombo->addItem(QStringLiteral("30 fps"), 30);
    m_fpsCombo->addItem(QStringLiteral("60 fps"), 60);

    auto *refreshBtn = new QPushButton(tr("マイク一覧を更新"), this);
    connect(refreshBtn, &QPushButton::clicked, this, [this] {
        refreshAudioDevices(m_micCombo->currentData().toString());
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { save(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *form = new QFormLayout(this);
    form->addRow(tr("録画モニター:"), m_screenCombo);
    form->addRow(tr("マイク:"), m_micCombo);
    form->addRow(QString(), refreshBtn);
    form->addRow(tr("フレームレート:"), m_fpsCombo);
    auto *note = new QLabel(tr("出力: H.264 + Opus / MP4、保存先はアプリと同じフォルダ"), this);
    note->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(note);
    form->addRow(buttons);
}

void SettingsDialog::refreshAudioDevices(const QString &select)
{
    m_micCombo->clear();
    m_micCombo->addItem(tr("(音声なし)"), QString());
    if (!Recorder::ffmpegExists()) {
        m_micCombo->addItem(tr("(ffmpeg.exe が見つかりません)"), QString());
        return;
    }
    const QStringList devs = Recorder::listAudioDevices();
    for (const QString &d : devs)
        m_micCombo->addItem(d, d);

    const int idx = m_micCombo->findData(select);
    m_micCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void SettingsDialog::load()
{
    QSettings st(settingsFilePath(), QSettings::IniFormat);

    const QString screenName = st.value(QStringLiteral("screen")).toString();
    const int sIdx = m_screenCombo->findData(screenName);
    m_screenCombo->setCurrentIndex(sIdx >= 0 ? sIdx : 0);

    refreshAudioDevices(st.value(QStringLiteral("mic")).toString());

    const int fps = st.value(QStringLiteral("fps"), 30).toInt();
    const int fIdx = m_fpsCombo->findData(fps);
    m_fpsCombo->setCurrentIndex(fIdx >= 0 ? fIdx : 1);
}

void SettingsDialog::save()
{
    QSettings st(settingsFilePath(), QSettings::IniFormat);
    st.setValue(QStringLiteral("screen"), m_screenCombo->currentData().toString());
    st.setValue(QStringLiteral("mic"),    m_micCombo->currentData().toString());
    st.setValue(QStringLiteral("fps"),    m_fpsCombo->currentData().toInt());
}
