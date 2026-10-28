#pragma once

#include <QMenuBar>

class HoverActivatedMenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit HoverActivatedMenuBar(QWidget *parent = nullptr);

    /// Вызвать при закрытии меню (aboutToHide), чтобы снова требовался клик для активации
    void deactivate();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// Единая логика: если меню активировано и курсор над элементом — открыть его (вызывается и после клика, и при наведении).
    void updateActiveFromCursor();

    bool m_activated { false };
};
