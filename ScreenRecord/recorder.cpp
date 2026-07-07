#include "recorder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QPair>
#include <QRegularExpression>

Recorder::Recorder(QObject *parent)
    : QObject(parent)
{
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(5000);
    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        if (m_proc && m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
    });
}

bool Recorder::isRecording() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

QString Recorder::ffmpegPath()
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe");
#else
    return QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg");
#endif
}

bool Recorder::ffmpegExists()
{
    return QFileInfo::exists(ffmpegPath());
}

QString Recorder::outputDir()
{
    QString dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MACOS
    // .app バンドル内 (Contents/MacOS) なら、バンドルの隣に保存する
    const int i = dir.indexOf(QStringLiteral(".app/Contents/MacOS"));
    if (i > 0) {
        const int slash = dir.lastIndexOf(QLatin1Char('/'), i);
        if (slash > 0)
            dir = dir.left(slash);
    }
#endif
    return dir;
}

// -list_devices の出力 (stderr) を取得
static QString runListDevices()
{
    QProcess p;
#ifdef Q_OS_WIN
    p.start(Recorder::ffmpegPath(),
            { QStringLiteral("-hide_banner"),
              QStringLiteral("-list_devices"), QStringLiteral("true"),
              QStringLiteral("-f"), QStringLiteral("dshow"),
              QStringLiteral("-i"), QStringLiteral("dummy") });
#else
    p.start(Recorder::ffmpegPath(),
            { QStringLiteral("-hide_banner"),
              QStringLiteral("-f"), QStringLiteral("avfoundation"),
              QStringLiteral("-list_devices"), QStringLiteral("true"),
              QStringLiteral("-i"), QString() });
#endif
    if (!p.waitForFinished(8000)) {
        p.kill();
        p.waitForFinished(1000);
    }
    return QString::fromUtf8(p.readAllStandardError());
}

#ifndef Q_OS_WIN
// avfoundation の一覧はセクション形式:
//   [AVFoundation indev @ ...] AVFoundation video devices:
//   [AVFoundation indev @ ...] [0] FaceTime HD Camera
//   [AVFoundation indev @ ...] [1] Capture screen 0
//   [AVFoundation indev @ ...] AVFoundation audio devices:
//   [AVFoundation indev @ ...] [0] Built-in Microphone

// audio セクションの (index, name) を列挙
static QList<QPair<int, QString>> avfAudioDevices(const QString &out)
{
    QList<QPair<int, QString>> result;
    bool inAudio = false;
    static const QRegularExpression re(QStringLiteral("\\[(\\d+)\\]\\s+(.+)$"));
    const QStringList lines = out.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.contains(QStringLiteral("audio devices"))) { inAudio = true;  continue; }
        if (line.contains(QStringLiteral("video devices"))) { inAudio = false; continue; }
        if (!inAudio)
            continue;
        const auto m = re.match(line);
        if (m.hasMatch())
            result.append({ m.captured(1).toInt(), m.captured(2).trimmed() });
    }
    return result;
}

// "Capture screen N" (N = Qtのスクリーン順に対応すると仮定) の avfoundation 側インデックス
static int avfScreenIndex(const QString &out, int qtScreenIndex)
{
    static const QRegularExpression re(
        QStringLiteral("\\[(\\d+)\\]\\s+Capture screen (\\d+)"));
    auto it = re.globalMatch(out);
    int firstFound = -1;
    while (it.hasNext()) {
        const auto m = it.next();
        if (firstFound < 0)
            firstFound = m.captured(1).toInt();
        if (m.captured(2).toInt() == qtScreenIndex)
            return m.captured(1).toInt();
    }
    return firstFound;   // 見つからなければ先頭の画面デバイス
}
#endif // !Q_OS_WIN

QStringList Recorder::listAudioDevices()
{
    QStringList result;
    if (!ffmpegExists())
        return result;

    const QString out = runListDevices();
#ifdef Q_OS_WIN
    // 例: [dshow @ 000001] "マイク (Realtek Audio)" (audio)
    static const QRegularExpression re(QStringLiteral("\"([^\"]+)\"\\s*\\(audio\\)"));
    auto it = re.globalMatch(out);
    while (it.hasNext())
        result << it.next().captured(1);
#else
    const auto devs = avfAudioDevices(out);
    for (const auto &d : devs)
        result << d.second;
#endif
    return result;
}

QString Recorder::makeOutputPath(const QString &ext)
{
    const QString dir  = outputDir();
    const QString base = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString path = dir + QLatin1Char('/') + base + QLatin1Char('.') + ext;
    int n = 2;
    while (QFileInfo::exists(path))
        path = dir + QLatin1Char('/') + base + QStringLiteral("_%1.").arg(n++) + ext;
    return path;
}

void Recorder::start(const QRect &physicalRect, int screenIndex,
                     const QString &audioDevice, int fps)
{
    if (isRecording() || !ffmpegExists())
        return;

    m_outFile = makeOutputPath(QStringLiteral("mp4"));
    m_errLog.clear();

    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-y");

#ifdef Q_OS_WIN
    Q_UNUSED(screenIndex);
    // --- 映像入力: gdigrab / 音声入力: dshow ---
    // 幅・高さは偶数に丸める (H.264 yuv420p の制約)
    const int w = physicalRect.width()  & ~1;
    const int h = physicalRect.height() & ~1;
    args << QStringLiteral("-f") << QStringLiteral("gdigrab")
         << QStringLiteral("-framerate") << QString::number(fps)
         << QStringLiteral("-offset_x") << QString::number(physicalRect.x())
         << QStringLiteral("-offset_y") << QString::number(physicalRect.y())
         << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(w).arg(h)
         << QStringLiteral("-draw_mouse") << QStringLiteral("1")
         << QStringLiteral("-i") << QStringLiteral("desktop");
    if (!audioDevice.isEmpty()) {
        args << QStringLiteral("-f") << QStringLiteral("dshow")
             << QStringLiteral("-rtbufsize") << QStringLiteral("100M")
             << QStringLiteral("-thread_queue_size") << QStringLiteral("512")
             << QStringLiteral("-i") << QStringLiteral("audio=") + audioDevice;
    }
#else
    // --- macOS: avfoundation は "映像インデックス:音声インデックス" の1入力 ---
    Q_UNUSED(physicalRect);   // 画面全体をキャプチャ (領域指定はcropフィルタで可能)
    const QString devList = runListDevices();
    const int scrIdx = avfScreenIndex(devList, screenIndex);
    int micIdx = -1;
    if (!audioDevice.isEmpty()) {
        const auto devs = avfAudioDevices(devList);
        for (const auto &d : devs)
            if (d.second == audioDevice) { micIdx = d.first; break; }
    }
    QString input = QString::number(scrIdx);
    if (micIdx >= 0)
        input += QLatin1Char(':') + QString::number(micIdx);

    args << QStringLiteral("-f") << QStringLiteral("avfoundation")
         << QStringLiteral("-framerate") << QString::number(fps)
         << QStringLiteral("-capture_cursor") << QStringLiteral("1")
         << QStringLiteral("-i") << input;
#endif

    // --- エンコード: H.264 + Opus (共通) ---
    args << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-crf") << QStringLiteral("23")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         // Retinaディスプレイ等で奇数サイズになった場合の保険
         << QStringLiteral("-vf") << QStringLiteral("crop=trunc(iw/2)*2:trunc(ih/2)*2");
    if (!audioDevice.isEmpty()) {
        args << QStringLiteral("-c:a") << QStringLiteral("libopus")
             << QStringLiteral("-b:a") << QStringLiteral("128k")
             << QStringLiteral("-ar") << QStringLiteral("48000");
    }
    // Opus in MP4 は ffmpeg 上は experimental 扱いのため -strict -2 を付ける
    args << QStringLiteral("-strict") << QStringLiteral("-2")
         << QStringLiteral("-movflags") << QStringLiteral("+faststart")
         << m_outFile;

    m_proc = new QProcess(this);
    m_proc->setProgram(ffmpegPath());
    m_proc->setArguments(args);

    connect(m_proc, &QProcess::readyReadStandardError, this, [this] {
        m_errLog += m_proc->readAllStandardError();
        if (m_errLog.size() > 16384)                 // ログは末尾のみ保持
            m_errLog = m_errLog.right(8192);
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Recorder::onProcessFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_proc && m_proc->state() == QProcess::NotRunning
            && m_proc->error() == QProcess::FailedToStart) {
            emit finished(m_outFile, false,
                          QStringLiteral("ffmpeg を起動できませんでした: ") + ffmpegPath());
            m_proc->deleteLater();
            m_proc = nullptr;
        }
    });

    m_proc->start();
    if (m_proc->waitForStarted(3000))
        emit started();
}

void Recorder::stop()
{
    if (!isRecording())
        return;
    // 'q' 送信で ffmpeg を正常終了させる (mp4 の moov を確実に書き込む)
    m_proc->write("q\n");
    m_killTimer.start();
}

void Recorder::stopAndWait(int timeoutMs)
{
    if (!isRecording())
        return;
    m_proc->write("q\n");
    if (!m_proc->waitForFinished(timeoutMs)) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

void Recorder::onProcessFinished(int exitCode, QProcess::ExitStatus st)
{
    m_killTimer.stop();
    const bool ok = (st == QProcess::NormalExit) && exitCode == 0
                    && QFileInfo::exists(m_outFile) && QFileInfo(m_outFile).size() > 0;
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    emit finished(m_outFile, ok, QString::fromUtf8(m_errLog));
}
