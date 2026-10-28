#pragma once

#include <QVector>
#include <QWidget>
#include <QPointF>
#include <QSize>

class QTimer;

class DemoTurtleCanvas : public QWidget {
    Q_OBJECT
public:
    explicit DemoTurtleCanvas(QWidget *parent = nullptr);

    void setSamplePath(const QVector<QPointF> &points);
    void playPath(const QVector<QPointF> &points, int durationMs = 0);
    void resetStudent();

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void playbackFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPointF toWidget(const QPointF &logical) const;
    void tick();

    QVector<QPointF> m_sample;
    QVector<QPointF> m_path;
    QVector<QPointF> m_drawn;
    QPointF m_turtlePos;
    double m_heading { 0.0 };
    int m_seg { 0 };
    double m_segT { 0.0 };
    bool m_playing { false };
    double m_unitsPerTick { 2.4 };
    QTimer *m_timer { nullptr };
    int m_logicalW { 400 };
    int m_logicalH { 300 };
};
