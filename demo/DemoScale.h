#pragma once

#include <QtGlobal>
#include <QSize>

constexpr qreal kDemoUiScale = 1.5;

inline int ds(int value)
{
    return qRound(value * kDemoUiScale);
}

inline QSize dsSize(int w, int h)
{
    return QSize(ds(w), ds(h));
}
