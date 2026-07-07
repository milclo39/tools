#include "recorder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
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
    return QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe");
}

bool Recorder::ffmpegExists()
{
    return QFileInfo::exists(ffmpegPath());
}

QStringList Recorder::listAudioDevices()
{
    QStringList result;
    if (!ffmpegExists())
        return result;

    QProcess p;
    p.start(ffmpegPath(), { QStringLiteral("-hide_banner"),
                            QStringLiteral("-list_devices"), QStringLiteral("true"),
                            QStringLiteral("-f"), QStringLiteral("dshow"),
                            QStringLiteral("-i"), QStringLiteral("dummy") });
    if (!p.waitForFinished(8000)) {
        p.kill();
        p.waitForFinished(1000);
    }
    // デバイス一覧は stderr に出力される (UTF-8)
    const QString out = QString::fromUtf8(p.readAllStandardError());
    // 例: [dshow @ 000001] "マイク (Realtek Audio)" (audio)
    static const QRegularExpression re(QStringLiteral("\"([^\"]+)\"\\s*\\(audio\\)"));
    auto it = re.globalMatch(out);
    while (it.hasNext())
        result << it.next().captured(1);
    return result;
}

QString Recorder::makeOutputPath(const QString &ext)
{
    const QString dir  = QCoreApplication::applicationDirPath();
    const QString base = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString path = dir + QLatin1Char('/') + base + QLatin1Char('.') + ext;
    int n = 2;
    while (QFileInfo::exists(path))
        path = dir + QLatin1Char('/') + base + QStringLiteral("_%1.").arg(n++) + ext;
    return path;
}

void Recorder::start(const QRect &physicalRect, const QString &audioDevice, int fps)
{
    if (isRecording() || !ffmpegExists())
        return;

    m_outFile = makeOutputPath(QStringLiteral("mp4"));
    m_errLog.clear();

    // 幅・高さは偶数に丸める (H.264 yuv420p の制約)
    const int w = physicalRect.width()  & ~1;
    const int h = physicalRect.height() & ~1;

    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-y")
         // --- 映像入力: 画面キャプチャ ---
         << QStringLiteral("-f") << QStringLiteral("gdigrab")
         << QStringLiteral("-framerate") << QString::number(fps)
         << QStringLiteral("-offset_x") << QString::number(physicalRect.x())
         << QStringLiteral("-offset_y") << QString::number(physicalRect.y())
         << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(w).arg(h)
         << QStringLiteral("-draw_mouse") << QStringLiteral("1")
         << QStringLiteral("-i") << QStringLiteral("desktop");

    // --- 音声入力: マイク (任意) ---
    if (!audioDevice.isEmpty()) {
        args << QStringLiteral("-f") << QStringLiteral("dshow")
             << QStringLiteral("-rtbufsize") << QStringLiteral("100M")
             << QStringLiteral("-thread_queue_size") << QStringLiteral("512")
             << QStringLiteral("-i") << QStringLiteral("audio=") + audioDevice;
    }

    // --- エンコード: H.264 + Opus ---
    args << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-crf") << QStringLiteral("23")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
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
                          QStringLiteral("ffmpeg.exe を起動できませんでした: ") + ffmpegPath());
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
