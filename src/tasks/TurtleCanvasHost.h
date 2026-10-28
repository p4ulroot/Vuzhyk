#pragma once

#include <QWidget>
#include <QColor>
#include <QString>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>

class QPainter;

struct FvtCanvasItem {
    int id { 0 };
    QString type;
    QVector<double> coords;
    QString fill;
    QString outline;
    double width { 1.0 };
    QString capstyle;
    QString text;
    QString anchor;
    QString fontFamily { QStringLiteral("Arial") };
    int fontSize { 8 };
    QString fontWeight;
    QString imagePath;
};

class TurtleCanvasHost : public QWidget {
    Q_OBJECT
public:
    explicit TurtleCanvasHost(QWidget *parent = nullptr);

    void setSampleForward(int pixels);
    void setLogicalCanvasSize(int width, int height);
    void applyScene(const QJsonObject &scene);
    bool hasScene() const { return m_hasScene; }

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void fvtEvent(const QJsonObject &event);

private:
    void drawScene(QPainter &p, const QRect &box) const;
    QPointF toWidget(double cx, double cy, const QRect &box) const;
    QPointF widgetToCanvas(const QPoint &pos) const;
    QColor parseColor(const QString &name, bool *ok = nullptr) const;
    Qt::Alignment alignmentFromAnchor(const QString &anchor) const;

    int m_sampleForward { 100 };
    int m_logicalWidth { 400 };
    int m_logicalHeight { 300 };
    bool m_hasScene { false };
    QString m_bg { QStringLiteral("white") };
    QVector<FvtCanvasItem> m_items;
};
