#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QVector>

class QEvent;
class QResizeEvent;
class QMenuBar;
class QTabWidget;
class QPropertyAnimation;
class QWidget;
class QTreeWidgetItem;
class HelpWidget;
class DemoCursor;
class DemoRecorder;

class DemoHelpWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DemoHelpWindow(QWidget *parent = nullptr);
    ~DemoHelpWindow() override;

    void beginHelpDemo();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum AfterMove {
        AfterNone,
        AfterClickItem,
        AfterGotoBrowser
    };

    void buildChrome();
    void prepareGrade6();
    void goToCurrentItem();
    void clickCurrentItem();
    void startZoom();
    void applyBrowserZoom(qreal size);
    qreal browserFontSize() const;
    void scrollThenNext();
    void advanceItem();
    void moveCursorTo(const QPoint &end, AfterMove after);
    void onCursorArrived();
    void finishDemo();
    QPoint itemHotPos(QTreeWidgetItem *item) const;
    QPoint browserHotPos() const;
    QTreeWidgetItem *currentItem() const;

    QMenuBar *m_menuBar { nullptr };
    QWidget *m_titleBar { nullptr };
    QTabWidget *m_tabs { nullptr };
    HelpWidget *m_help { nullptr };
    DemoCursor *m_cursor { nullptr };
    DemoRecorder *m_recorder { nullptr };
    QPropertyAnimation *m_cursorAnim { nullptr };
    QVector<QString> m_keys;
    int m_itemIndex { 0 };
    AfterMove m_afterMove { AfterNone };
    bool m_ignoreCursorFinish { false };
    QPoint m_dragOffset;
    bool m_dragging { false };
};
