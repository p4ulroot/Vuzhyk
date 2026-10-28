#include "HoverActivatedMenuBar.h"

#include <QAction>
#include <QApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QCursor>
#include <QGuiApplication>

namespace {
/// Задержка перед первым открытием после клика, чтобы анимация шла так же, как при наведении (без лагов).
constexpr int kFirstOpenDelayMs = 40;
}

HoverActivatedMenuBar::HoverActivatedMenuBar(QWidget *parent)
    : QMenuBar(parent) {
    connect(this, &QMenuBar::hovered, this, [this](QAction *) {
        updateActiveFromCursor();
    });
}

void HoverActivatedMenuBar::deactivate() {
    m_activated = false;
}

void HoverActivatedMenuBar::updateActiveFromCursor() {
    if (!m_activated) return;
    QAction *a = actionAt(mapFromGlobal(QCursor::pos()));
    if (a) setActiveAction(a);
}

void HoverActivatedMenuBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        QAction *clicked = actionAt(event->pos());
        // Пункты без меню (например «Агент») — обычная кнопка, срабатывает сразу
        if (clicked && !clicked->menu()) {
            clicked->trigger();
            event->accept();
            return;
        }

        m_activated = true;
        QTimer::singleShot(kFirstOpenDelayMs, this, [this]() {
            if (!m_activated) return;
            QPoint pos = mapFromGlobal(QCursor::pos());
            QMouseEvent moveEv(QEvent::MouseMove, pos, QCursor::pos(), Qt::NoButton, Qt::NoButton, QGuiApplication::keyboardModifiers());
            QApplication::sendEvent(this, &moveEv);
            updateActiveFromCursor();
        });
        event->accept();
        return;
    }
    QMenuBar::mousePressEvent(event);
}
