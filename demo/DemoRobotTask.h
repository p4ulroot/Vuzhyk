#pragma once

#include <QWidget>
#include <QVector>

class QLabel;
class QSpinBox;
class QComboBox;
class QToolBar;
class QToolButton;
class QButtonGroup;
class DemoRobotCanvas;

class DemoRobotTask : public QWidget {
    Q_OBJECT
public:
    enum Tool {
        PaintEmpty = 0,
        PaintToBeFilled = 1,
        PaintFilled = 2,
        SetStart = 3,
        SetParking = 4,
        ToggleWall = 5
    };

    explicit DemoRobotTask(QWidget *parent = nullptr);

    DemoRobotCanvas *canvas() const { return m_canvas; }
    QToolButton *toolButton(int id) const;
    void setToolChecked(int id);

private:
    void createToolbar();
    QIcon makeToolIcon(const QColor &fill, bool ring, bool dot, bool wall) const;

    DemoRobotCanvas *m_canvas { nullptr };
    QToolBar *m_toolBar { nullptr };
    QButtonGroup *m_toolGroup { nullptr };
    QSpinBox *m_rowsSpin { nullptr };
    QSpinBox *m_colsSpin { nullptr };
    QSpinBox *m_variantsSpin { nullptr };
    QSpinBox *m_checksSpin { nullptr };
    QComboBox *m_variantCombo { nullptr };
};
