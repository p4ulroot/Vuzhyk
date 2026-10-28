#pragma once

#include <QMenu>
#include <QVariantAnimation>

class AnimatedMenu : public QMenu {
    Q_OBJECT
public:
    explicit AnimatedMenu(QWidget *parent = nullptr);
    AnimatedMenu(const QString &title, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

private:
    void initializeAnimation();
    void startOpenAnimation();
    void startCloseAnimation();
    void applyClip(int visibleHeight);

    QVariantAnimation *m_animation { nullptr };
    bool m_blockClose { false };
    bool m_closing { false };
};
