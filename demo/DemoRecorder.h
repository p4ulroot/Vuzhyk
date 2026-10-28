#pragma once

#include <QObject>
#include <QString>
#include <QElapsedTimer>

class QWidget;
class QTimer;
class DemoEncodeThread;

class DemoRecorder : public QObject {
    Q_OBJECT
public:
    explicit DemoRecorder(QObject *parent = nullptr);
    ~DemoRecorder() override;

    bool start(QWidget *window, const QString &filePrefix = QStringLiteral("DemoTurtleHouse"));
    void stop();
    bool isRunning() const { return m_running; }
    QString outputPath() const { return m_outputPath; }

private:
    void onTick();

    QWidget *m_window { nullptr };
    QTimer *m_timer { nullptr };
    DemoEncodeThread *m_thread { nullptr };
    QString m_outputPath;
    bool m_running { false };
    qint64 m_frameIndex { 0 };
    QElapsedTimer m_clock;
};
