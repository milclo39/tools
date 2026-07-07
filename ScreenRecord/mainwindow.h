#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QElapsedTimer>
#include <QPoint>
#include <QTimer>
#include <QWidget>

class QLabel;
class QToolButton;
class Recorder;
class QScreen;

// 常に最前面に浮かぶ小型ツールバー
//  [● REC] [📷] [⚙] [✕]  + 経過時間表示
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
    void toggleRecording();
    void takeScreenshot();
    void openSettings();
    void updateElapsed();

private:
    QScreen *targetScreen() const;   // 設定で選ばれたモニター
    QRect targetPhysicalRect() const;
    void setRecordingUi(bool recording);
    void flashMessage(const QString &msg);

    Recorder    *m_recorder = nullptr;
    QToolButton *m_recBtn   = nullptr;
    QToolButton *m_shotBtn  = nullptr;
    QToolButton *m_cfgBtn   = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QLabel      *m_timeLabel = nullptr;

    QTimer        m_uiTimer;
    QElapsedTimer m_elapsed;
    QPoint        m_dragOffset;
};

#endif // MAINWINDOW_H
