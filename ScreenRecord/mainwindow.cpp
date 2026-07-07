#include "mainwindow.h"
#include "recorder.h"
#include "settingsdialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPainter>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

#ifdef Q_OS_WIN
#  include <windows.h>
#  ifndef WDA_EXCLUDEFROMCAPTURE
#    define WDA_EXCLUDEFROMCAPTURE 0x00000011
#  endif
#endif
#ifdef Q_OS_MACOS
#  include "machelper.h"
#endif

namespace {
const char *kButtonStyle =
    "QToolButton { color: white; background: transparent; border: none;"
    "              font-size: 15px; padding: 4px 6px; border-radius: 6px; }"
    "QToolButton:hover { background: rgba(255,255,255,40); }";
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    // 枠なし・常に最前面・タスクバー非表示の小型ウィンドウ
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);   // 角丸描画のため
    setWindowTitle(QStringLiteral("ScreenRecord"));

    m_recorder = new Recorder(this);

    m_recBtn   = new QToolButton(this);
    m_shotBtn  = new QToolButton(this);
    m_cfgBtn   = new QToolButton(this);
    m_closeBtn = new QToolButton(this);
    m_timeLabel = new QLabel(QStringLiteral("--:--"), this);

    m_recBtn->setText(QStringLiteral("●"));
    m_recBtn->setToolTip(tr("録画開始 / 停止"));
    m_shotBtn->setText(QStringLiteral("📷"));
    m_shotBtn->setToolTip(tr("静止画キャプチャ (PNG)"));
    m_cfgBtn->setText(QStringLiteral("⚙"));
    m_cfgBtn->setToolTip(tr("設定"));
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setToolTip(tr("終了"));

    for (QToolButton *b : { m_recBtn, m_shotBtn, m_cfgBtn, m_closeBtn })
        b->setStyleSheet(QString::fromLatin1(kButtonStyle));
    m_recBtn->setStyleSheet(QString::fromLatin1(kButtonStyle)
                            + QStringLiteral("QToolButton { color: #ff5252; }"));
    m_timeLabel->setStyleSheet(
        QStringLiteral("color: #dddddd; font-family: Consolas, monospace; font-size: 13px;"));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 6, 10, 6);
    lay->setSpacing(2);
    lay->addWidget(m_recBtn);
    lay->addWidget(m_timeLabel);
    lay->addWidget(m_shotBtn);
    lay->addWidget(m_cfgBtn);
    lay->addWidget(m_closeBtn);
    setFixedSize(sizeHint());

    connect(m_recBtn,   &QToolButton::clicked, this, &MainWindow::toggleRecording);
    connect(m_shotBtn,  &QToolButton::clicked, this, &MainWindow::takeScreenshot);
    connect(m_cfgBtn,   &QToolButton::clicked, this, &MainWindow::openSettings);
    connect(m_closeBtn, &QToolButton::clicked, this, &QWidget::close);

    m_uiTimer.setInterval(500);
    connect(&m_uiTimer, &QTimer::timeout, this, &MainWindow::updateElapsed);

    connect(m_recorder, &Recorder::started, this, [this] {
        m_elapsed.start();
        m_uiTimer.start();
        setRecordingUi(true);
    });
    connect(m_recorder, &Recorder::finished, this,
            [this](const QString &file, bool ok, const QString &log) {
        m_uiTimer.stop();
        setRecordingUi(false);
        if (ok) {
            flashMessage(tr("保存しました: %1").arg(QFileInfo(file).fileName()));
        } else {
            QMessageBox::warning(this, tr("録画エラー"),
                                 tr("録画に失敗しました。\n\n%1")
                                     .arg(log.right(1500)));
        }
    });

    // 画面右下に配置
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    move(avail.right() - width() - 24, avail.bottom() - height() - 24);

    // このツールバー自体を画面キャプチャから除外する
    // (録画映像・スクリーンショットに写り込まなくなる)
#ifdef Q_OS_WIN
    // Windows 10 2004 以降
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), WDA_EXCLUDEFROMCAPTURE);
#endif
#ifdef Q_OS_MACOS
    macExcludeWindowFromCapture(winId());
#endif
}

QScreen *MainWindow::targetScreen() const
{
    QSettings st(SettingsDialog::settingsFilePath(), QSettings::IniFormat);
    const QString name = st.value(QStringLiteral("screen")).toString();
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens)
        if (s->name() == name)
            return s;
    return QGuiApplication::primaryScreen();
}

QRect MainWindow::targetPhysicalRect() const
{
    // Qt の論理座標 -> 物理ピクセル (gdigrab は物理ピクセルで指定する)
    QScreen *s = targetScreen();
    const qreal dpr = s->devicePixelRatio();
    const QRect g = s->geometry();
    return QRect(qRound(g.x() * dpr), qRound(g.y() * dpr),
                 qRound(g.width() * dpr), qRound(g.height() * dpr));
}

void MainWindow::toggleRecording()
{
    if (m_recorder->isRecording()) {
        m_recorder->stop();
        m_recBtn->setEnabled(false);   // finished シグナルまで多重操作を防ぐ
        return;
    }

    if (!Recorder::ffmpegExists()) {
#ifdef Q_OS_WIN
        const QString hint = tr("入手先: https://www.gyan.dev/ffmpeg/builds/ の\n"
                                "\"release essentials\" ビルド (libx264 / libopus 同梱) を推奨します。");
#else
        const QString hint = tr("入手先: https://evermeet.cx/ffmpeg/ の静的ビルド、\n"
                                "または Homebrew (brew install ffmpeg) のバイナリをコピーしてください。");
#endif
        QMessageBox::warning(this, tr("ffmpeg が見つかりません"),
            tr("ffmpeg をアプリと同じフォルダに置いてください。\n\n%1\n\n%2")
                .arg(Recorder::ffmpegPath(), hint));
        return;
    }

    QSettings st(SettingsDialog::settingsFilePath(), QSettings::IniFormat);
    const QString mic = st.value(QStringLiteral("mic")).toString();
    const int fps     = st.value(QStringLiteral("fps"), 30).toInt();

    const int screenIndex = QGuiApplication::screens().indexOf(targetScreen());
    m_recorder->start(targetPhysicalRect(), qMax(0, screenIndex), mic, fps);
}

void MainWindow::setRecordingUi(bool recording)
{
    m_recBtn->setEnabled(true);
    m_recBtn->setText(recording ? QStringLiteral("■") : QStringLiteral("●"));
    m_recBtn->setToolTip(recording ? tr("録画停止") : tr("録画開始"));
    m_timeLabel->setText(recording ? QStringLiteral("00:00") : QStringLiteral("--:--"));
    m_cfgBtn->setEnabled(!recording);
}

void MainWindow::updateElapsed()
{
    const qint64 s = m_elapsed.elapsed() / 1000;
    m_timeLabel->setText(QStringLiteral("%1:%2")
                             .arg(s / 60, 2, 10, QLatin1Char('0'))
                             .arg(s % 60, 2, 10, QLatin1Char('0')));
}

void MainWindow::takeScreenshot()
{
    QScreen *s = targetScreen();
    // WDA_EXCLUDEFROMCAPTURE が効かない環境 (Win10 2004 未満) 向けに一旦隠す
    hide();
    QTimer::singleShot(250, this, [this, s] {
        const QPixmap pix = s->grabWindow(0);
        show();
        if (pix.isNull()) {
            QMessageBox::warning(this, tr("キャプチャ失敗"), tr("画面を取得できませんでした。"));
            return;
        }
        const QString path = Recorder::makeOutputPath(QStringLiteral("png"));
        if (pix.save(path, "PNG"))
            flashMessage(tr("保存しました: %1").arg(QFileInfo(path).fileName()));
        else
            QMessageBox::warning(this, tr("キャプチャ失敗"), tr("PNG の保存に失敗しました。"));
    });
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    dlg.load();
    dlg.exec();
}

void MainWindow::flashMessage(const QString &msg)
{
    QToolTip::showText(frameGeometry().topLeft() - QPoint(0, 28), msg, this, QRect(), 3000);
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 30, 230));
    p.drawRoundedRect(rect(), 10, 10);
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
    QWidget::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton)
        move(e->globalPosition().toPoint() - m_dragOffset);
    QWidget::mouseMoveEvent(e);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_recorder->isRecording())
        m_recorder->stopAndWait(5000);   // moov 書き込みを待ってから終了
    e->accept();
}
