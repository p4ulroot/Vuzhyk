#include "DemoRobotTask.h"
#include "DemoRobotCanvas.h"
#include "DemoScale.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

DemoRobotTask::DemoRobotTask(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("pyrobEditorWidget"));
    createToolbar();
    m_canvas = new DemoRobotCanvas(this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolBar);
    layout->addWidget(m_canvas, 1);
}

QIcon DemoRobotTask::makeToolIcon(const QColor &fill, bool ring, bool dot, bool wall) const
{
    const int s = ds(24);
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect r(ds(3), ds(3), ds(18), ds(18));
    if (fill.isValid()) {
        p.fillRect(r, fill);
        p.setPen(QPen(Qt::gray, 1));
        p.drawRect(r);
    }
    if (ring) {
        p.setPen(QPen(Qt::blue, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r.adjusted(ds(4), ds(4), -ds(4), -ds(4)));
    }
    if (dot) {
        p.setBrush(Qt::black);
        p.setPen(Qt::NoPen);
        const int d = ds(6);
        p.drawEllipse(QRect(r.center().x() - d / 2, r.center().y() - d / 2, d, d));
    }
    if (wall) {
        p.setPen(QPen(QColor(120, 120, 120), 3));
        p.drawLine(r.right(), r.top(), r.right(), r.bottom());
    }
    return QIcon(pm);
}

void DemoRobotTask::createToolbar()
{
    m_toolBar = new QToolBar(QString::fromUtf8("Основное"), this);
    m_toolBar->setOrientation(Qt::Vertical);
    m_toolBar->setMovable(false);
    m_toolBar->setMinimumWidth(ds(66));
    m_toolBar->setMaximumWidth(ds(66));
    m_toolBar->setStyleSheet(QStringLiteral(
        "QToolBar { background-color: #353535; border: none; }"
        "QLabel { color: #ffffff; }"
        "QToolButton { background-color: transparent; color: #ffffff; border: 1px solid transparent; }"
        "QToolButton:hover { background-color: #555555; border: 1px solid #777777; }"
        "QToolButton:checked { background-color: #2a82da; border: 1px solid #2a82da; }"
        "QToolButton:checked:hover { background-color: #3a92ea; }"
        "QSpinBox, QComboBox { background-color: #232323; color: #ffffff; border: 1px solid #555555; padding: 2px; }"
        "QSpinBox:hover, QComboBox:hover { border: 1px solid #777777; }"
        "QSpinBox:focus, QComboBox:focus { border: 1px solid #2a82da; }"
        "QComboBox::drop-down { border: none; background-color: #353535; }"
        "QComboBox QAbstractItemView { background-color: #232323; color: #ffffff;"
        " selection-background-color: #2a82da; border: 1px solid #555555; }"
        "QMenu { background-color: #353535; color: #ffffff; border: 1px solid #555555; }"
        "QMenu::item:selected { background-color: #555555; }"));

    auto *fileMenu = new QMenu(this);
    fileMenu->setTitle(QString::fromUtf8("Файл"));
    fileMenu->addAction(QString::fromUtf8("Новый"));
    fileMenu->addAction(QString::fromUtf8("Открыть..."));
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("Сохранить задачу (.py)"));
    fileMenu->addAction(QString::fromUtf8("Сохранить проект (.json)"));
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("Экспорт в Python..."));

    auto *fileButton = new QToolButton(this);
    fileButton->setText(QString::fromUtf8("Файл"));
    fileButton->setMenu(fileMenu);
    fileButton->setPopupMode(QToolButton::InstantPopup);
    fileButton->setMaximumWidth(ds(85));
    m_toolBar->addWidget(fileButton);
    m_toolBar->addSeparator();

    auto addSpin = [this](const QString &label, int value, int maxVal, QSpinBox **out) {
        auto *lbl = new QLabel(label, this);
        m_toolBar->addWidget(lbl);
        auto *spin = new QSpinBox(this);
        spin->setRange(1, maxVal);
        spin->setValue(value);
        spin->setMaximumWidth(ds(60));
        m_toolBar->addWidget(spin);
        *out = spin;
    };
    addSpin(QString::fromUtf8("Строки:"), 15, 200, &m_rowsSpin);
    addSpin(QString::fromUtf8("Столбцы:"), 15, 200, &m_colsSpin);
    addSpin(QString::fromUtf8("Варианты:"), 1, 100, &m_variantsSpin);

    auto *variantLbl = new QLabel(QString::fromUtf8("Вариант:"), this);
    m_toolBar->addWidget(variantLbl);
    m_variantCombo = new QComboBox(this);
    m_variantCombo->addItem(QStringLiteral("1"));
    m_variantCombo->setMaximumWidth(ds(60));
    m_toolBar->addWidget(m_variantCombo);

    addSpin(QString::fromUtf8("Проверок:"), 1, 1, &m_checksSpin);

    m_toolBar->addSeparator();
    auto *toolLbl = new QLabel(QString::fromUtf8("Инструмент:"), this);
    m_toolBar->addWidget(toolLbl);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);

    struct ToolDef { int id; QColor fill; bool ring; bool dot; bool wall; const char *tip; };
    const ToolDef row1[] = {
        { PaintToBeFilled, QColor(48, 186, 176), false, false, false, "Закрасить" },
        { PaintFilled, QColor(255, 248, 209), false, false, false, "Закрашена" }
    };
    const ToolDef row2[] = {
        { PaintEmpty, QColor(255, 255, 255), false, false, false, "Пустая" },
        { ToggleWall, QColor(), false, false, true, "Стена" }
    };
    const ToolDef row3[] = {
        { SetStart, QColor(), true, false, false, "Старт" },
        { SetParking, QColor(), false, true, false, "Парковка" }
    };

    auto addRow = [this](const ToolDef *defs, int count) {
        auto *row = new QWidget(this);
        auto *lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(ds(2));
        for (int i = 0; i < count; ++i) {
            const ToolDef &d = defs[i];
            auto *btn = new QToolButton(this);
            btn->setCheckable(true);
            btn->setAutoExclusive(true);
            btn->setIcon(makeToolIcon(d.fill, d.ring, d.dot, d.wall));
            btn->setToolTip(QString::fromUtf8(d.tip));
            btn->setFixedSize(dsSize(28, 28));
            btn->setIconSize(dsSize(24, 24));
            lay->addWidget(btn);
            m_toolGroup->addButton(btn, d.id);
        }
        m_toolBar->addWidget(row);
    };
    addRow(row1, 2);
    addRow(row2, 2);
    addRow(row3, 2);

    if (QAbstractButton *b = m_toolGroup->button(PaintEmpty))
        b->setChecked(true);
}

QToolButton *DemoRobotTask::toolButton(int id) const
{
    if (!m_toolGroup)
        return nullptr;
    return qobject_cast<QToolButton *>(m_toolGroup->button(id));
}

void DemoRobotTask::setToolChecked(int id)
{
    if (!m_toolGroup)
        return;
    if (QAbstractButton *b = m_toolGroup->button(id))
        b->setChecked(true);
}
