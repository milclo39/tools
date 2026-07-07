#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <QProcess>
#include <QRect>
#include <QTimer>

// ffmpeg.exe をサブプロセスとして制御する録画エンジン
// 映像: gdigrab (画面キャプチャ) -> libx264
// 音声: dshow (マイク)          -> libopus
class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);

    bool isRecording() const;
    QString lastOutputFile() const { return m_outFile; }

    // ffmpeg.exe の場所 (アプリと同じフォルダ)
    static QString ffmpegPath();
    static bool ffmpegExists();

    // dshow のオーディオ入力デバイス名を列挙 (ffmpeg -list_devices)
    static QStringList listAudioDevices();

    // アプリと同じフォルダに yyyyMMdd_HHmmss ベースの重複しないパスを生成
    static QString makeOutputPath(const QString &ext);

public slots:
    // physicalRect: 物理ピクセルでのキャプチャ範囲 (仮想デスクトップ座標)
    // audioDevice : dshow デバイス名。空文字なら音声なし
    void start(const QRect &physicalRect, const QString &audioDevice, int fps);
    void stop();                    // 'q' 送信で正常終了 (moov 書き込み)
    void stopAndWait(int timeoutMs); // 終了時用: 完了までブロック

signals:
    void started();
    void finished(const QString &filePath, bool ok, const QString &errorLog);

private:
    void onProcessFinished(int exitCode, QProcess::ExitStatus st);

    QProcess  *m_proc = nullptr;
    QTimer     m_killTimer;   // 'q' が効かない場合の強制終了
    QString    m_outFile;
    QByteArray m_errLog;
};

#endif // RECORDER_H
