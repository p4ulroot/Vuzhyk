#include "AnimatedMenu.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QEvent>
#include <QRegion>
#include <QShowEvent>

#include <algorithm>

namespace {
constexpr int kAnimationDurationMs = 160;
}

AnimatedMenu::AnimatedMenu(QWidget *parent)
    : QMenu(parent)
{
    initializeAnimation();
}

AnimatedMenu::AnimatedMenu(const QString &title, QWidget *parent)
    : QMenu(title, parent)
{
    initializeAnimation();
}

void AnimatedMenu::applyClip(int visibleHeight)
{
    const int w = std::max({ 1, width(), sizeHint().width() });
    const int h = std::max(1, visibleHeight);
    setMask(QRegion(0, 0, w, h));
}

void AnimatedMenu::initializeAnimation()
{
    if (!m_animation)
        m_animation = new QVariantAnimation(this);
    m_animation->setDuration(kAnimationDurationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        applyClip(value.toInt());
    });

    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        if (m_closing) {
            m_blockClose = true;
            QMenu::hide();
            clearMask();
            m_blockClose = false;
            m_closing = false;
        } else {
            clearMask();
        }
    });
}

void AnimatedMenu::showEvent(QShowEvent *event)
{
    applyClip(1);
    QMenu::showEvent(event);
    startOpenAnimation();
}

bool AnimatedMenu::event(QEvent *event)
{
    if (event->type() == QEvent::Show && !m_closing)
        applyClip(1);

    if (event->type() == QEvent::Hide && !m_blockClose) {
        if (isVisible()) {
            event->ignore();
            startCloseAnimation();
            return true;
        }

        // Qt может скрыть popup напрямую. Сбрасываем маску и состояние,
        // чтобы следующий Show не оставил меню обрезанным после старой анимации.
        if (m_animation->state() == QAbstractAnimation::Running)
            m_animation->stop();
        clearMask();
        m_closing = false;
    }
    return QMenu::event(event);
}

void AnimatedMenu::startOpenAnimation()
{
    if (!isVisible())
        return;

    m_closing = false;
    if (m_animation->state() == QAbstractAnimation::Running)
        m_animation->stop();

    const int targetHeight = std::max({ 1, height(), sizeHint().height() });
    applyClip(1);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    m_animation->setStartValue(1);
    m_animation->setEndValue(targetHeight);
    m_animation->start();
}

void AnimatedMenu::startCloseAnimation()
{
    if (!isVisible())
        return;
    m_closing = true;

    if (m_animation->state() == QAbstractAnimation::Running)
        m_animation->stop();

    const int startHeight = std::max({ 1, height(), sizeHint().height() });
    applyClip(startHeight);
    m_animation->setEasingCurve(QEasingCurve::InCubic);
    m_animation->setStartValue(startHeight);
    m_animation->setEndValue(1);
    m_animation->start();
}
