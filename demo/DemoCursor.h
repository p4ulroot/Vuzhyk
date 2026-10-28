#pragma once

#include <QPixmap>
#include <QPoint>
#include <QWidget>

class DemoCursor : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QPoint hotPos READ hotPos WRITE setHotPos)
public:
    explicit DemoCursor(QWidget *parent = nullptr);

    QPoint hotPos() const { return m_hotPos; }
    void setHotPos(const QPoint &pos);
    void setPressed(bool pressed);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool loadFromCur(const QString &resourcePath);

    QPoint m_hotPos;
    QPoint m_hotspot { 1, 1 };
    QPixmap m_pixmap;
    bool m_pressed { false };
};
