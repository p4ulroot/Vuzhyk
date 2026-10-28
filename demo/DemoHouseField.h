#pragma once

#include <QPointF>
#include <QVector>
#include <QWidget>

class QTimer;
class DemoRecorder;

class DemoHouseField : public QWidget {
public:
    struct Seg {
        QPointF a;
        QPointF b;
        bool draw { true };
    };

    explicit DemoHouseField(QWidget *parent = nullptr);
    ~DemoHouseField() override;
    void beginRecording();
    void beginDrawing();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildPath();
    void tick();
    void finishAndClose();
    QPointF toWidget(const QPointF &logical) const;
    void drawAxes(class QPainter &p) const;
    void drawTurtle(class QPainter &p) const;

    QVector<Seg> m_segs;
    QVector<QVector<QPointF>> m_strokes;
    QPointF m_turtlePos { 0.0, 0.0 };
    double m_heading { 0.0 };
    int m_seg { 0 };
    double m_segT { 0.0 };
    bool m_playing { false };
    bool m_closing { false };
    QTimer *m_timer { nullptr };
    DemoRecorder *m_recorder { nullptr };
};
