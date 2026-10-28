#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>

class QEvent;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class DemoTurtleCanvas;
class DemoPythonHighlighter;

class DemoTurtleTask : public QWidget {
    Q_OBJECT
public:
    explicit DemoTurtleTask(QWidget *parent = nullptr);

    QPlainTextEdit *editor() const { return m_editor; }
    QPushButton *runButton() const { return m_runBtn; }
    DemoTurtleCanvas *canvas() const { return m_canvas; }
    QPushButton *catalogButton(int number) const;

    void setInstruction(const QString &text, const QString &kind = QStringLiteral("hint"));
    void setCatalogSelected(int number);
    void appendTyped(const QString &chunk);
    void setTheme(const QString &theme);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyTheme();
    void updateCanvasHeight();

    QSplitter *m_splitter { nullptr };
    QPlainTextEdit *m_editor { nullptr };
    QWidget *m_panel { nullptr };
    QLabel *m_instruction { nullptr };
    DemoTurtleCanvas *m_canvas { nullptr };
    QPushButton *m_runBtn { nullptr };
    QPushButton *m_stepBtn { nullptr };
    QPushButton *m_resetBtn { nullptr };
    QVector<QPushButton *> m_catalogButtons;
    QString m_kind { QStringLiteral("hint") };
    QString m_theme { QStringLiteral("light") };
    DemoPythonHighlighter *m_highlighter { nullptr };
};
