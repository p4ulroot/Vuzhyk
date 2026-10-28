#include "PyrobEditorWidget.h"
#include "grideditor.h"
#include "projectmodel.h"
#include "../MainWindow.h"
#include "../PythonInterpreterPaths.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QToolButton>
#include <QButtonGroup>
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSpinBox>
#include <QComboBox>
#include <QStyleOptionComboBox>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QVector>
#include <QSize>
#include <QCoreApplication>
#include <QApplication>
#include <QPalette>
#include <QTimer>
#include <QStyle>
#include <QProxyStyle>
#include <QStyleOption>
#include <QStyleOptionSpinBox>
#include <QStyleOptionButton>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QStandardPaths>

namespace {

QString documentsDirectory()
{
	const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	return docs.isEmpty() ? QDir::homePath() : docs;
}

} // namespace

PyrobEditorWidget::PyrobEditorWidget(QWidget *parent)
	: QWidget(parent)
	, m_editor(new GridEditor(this))
	, m_model(new ProjectModel(this))
	, m_mainWindow(qobject_cast<MainWindow*>(parent))
    , m_newAct(nullptr)
    , m_openAct(nullptr)
    , m_saveTaskAct(nullptr)
    , m_saveAct(nullptr)
	, m_exportAct(nullptr)
    , m_rowsSpin(nullptr)
    , m_colsSpin(nullptr)
    , m_variantsSpin(nullptr)
    , m_checksSpin(nullptr)
    , m_variantCombo(nullptr)
	, m_toolGroup(nullptr)
	, m_lang(LangRu)
	, m_fileMenu(nullptr)
	, m_fileButton(nullptr)
	, m_rowsLbl(nullptr)
	, m_colsLbl(nullptr)
	, m_variantsLbl(nullptr)
	, m_variantLbl(nullptr)
	, m_checksLbl(nullptr)
	, m_toolLbl(nullptr)
	, m_toolBar(nullptr)
{
	createUi();
	m_editor->setModel(m_model);
	// Используем слабую ссылку в lambda, чтобы избежать циклических ссылок
	connect(m_model, &ProjectModel::changed, this, [this]() {
		// Можно добавить индикатор изменений, если нужно
	});
	connect(m_model, &ProjectModel::variantSwitched, this, [this](int) {
		if (m_rowsSpin && m_colsSpin && m_model) {
			m_rowsSpin->blockSignals(true);
			m_colsSpin->blockSignals(true);
			m_rowsSpin->setValue(m_model->rows());
			m_colsSpin->setValue(m_model->cols());
			m_rowsSpin->blockSignals(false);
			m_colsSpin->blockSignals(false);
		}
	});
	// Устанавливаем тему по умолчанию (светлая)
	m_theme = "light";
}

PyrobEditorWidget::~PyrobEditorWidget() {
	// НЕ вызываем clearAll() в деструкторе, так как Qt автоматически удалит все дочерние виджеты
	// Вызов clearAll() может вызвать проблемы, если дочерние виджеты уже удалены
	// Просто отключаем все связи
	disconnect();
}

void PyrobEditorWidget::clearAll() {
	// Отключаем все связи явно
	if (m_model) {
		m_model->disconnect(this);
	}
	if (m_editor) {
		m_editor->disconnect(this);
	}
	if (m_toolGroup) {
		// Удаляем все кнопки из группы перед отключением
		QList<QAbstractButton*> buttons = m_toolGroup->buttons();
		for (QAbstractButton *btn : buttons) {
			m_toolGroup->removeButton(btn);
			// Отключаем все связи кнопки
			btn->disconnect();
			// Удаляем кнопку
			btn->setParent(nullptr);
			delete btn;
		}
		m_toolGroup->disconnect(this);
		m_toolGroup->setParent(nullptr);
		delete m_toolGroup;
		m_toolGroup = nullptr;
	}
	if (m_rowsSpin) {
		m_rowsSpin->disconnect(this);
	}
	if (m_colsSpin) {
		m_colsSpin->disconnect(this);
	}
	if (m_variantsSpin) {
		m_variantsSpin->disconnect(this);
	}
	if (m_variantCombo) {
		m_variantCombo->disconnect(this);
	}
	if (m_checksSpin) {
		m_checksSpin->disconnect(this);
	}
	if (m_newAct) {
		m_newAct->disconnect(this);
	}
	if (m_openAct) {
		m_openAct->disconnect(this);
	}
	if (m_saveTaskAct) {
		m_saveTaskAct->disconnect(this);
	}
	if (m_saveAct) {
		m_saveAct->disconnect(this);
	}
	if (m_exportAct) {
		m_exportAct->disconnect(this);
	}
	
	// Отключаем все остальные связи
	disconnect();
	
	// Очищаем указатели на дочерние объекты
	// Сначала очищаем editor и model, затем удаляем их
	if (m_editor) {
		// Явно очищаем редактор перед удалением
		m_editor->clear();
		m_editor->setModel(nullptr); // Отключаем модель от редактора
		// Отключаем все связи редактора
		m_editor->disconnect();
		m_editor->setParent(nullptr);
		delete m_editor;
		m_editor.clear();
	}
	if (m_model) {
		// Отключаем все связи модели
		m_model->disconnect();
		// Очищаем данные модели
		m_model->resizeGridSilent(0, 0);
		m_model->setVariantCount(0);
		m_model->setParent(nullptr);
		delete m_model;
		m_model.clear();
	}
	m_mainWindow = nullptr;
	
	// Явно удаляем все виджеты перед обнулением указателей
	// Сначала отключаем все связи, затем удаляем виджеты напрямую
	if (m_rowsSpin) {
		m_rowsSpin->disconnect();
		m_rowsSpin->setParent(nullptr);
		delete m_rowsSpin;
		m_rowsSpin = nullptr;
	}
	if (m_colsSpin) {
		m_colsSpin->disconnect();
		m_colsSpin->setParent(nullptr);
		delete m_colsSpin;
		m_colsSpin = nullptr;
	}
	if (m_variantsSpin) {
		m_variantsSpin->disconnect();
		m_variantsSpin->setParent(nullptr);
		delete m_variantsSpin;
		m_variantsSpin = nullptr;
	}
	if (m_checksSpin) {
		m_checksSpin->disconnect();
		m_checksSpin->setParent(nullptr);
		delete m_checksSpin;
		m_checksSpin = nullptr;
	}
	if (m_variantCombo) {
		m_variantCombo->disconnect();
		m_variantCombo->setParent(nullptr);
		delete m_variantCombo;
		m_variantCombo = nullptr;
	}
	if (m_rowsLbl) {
		m_rowsLbl->disconnect();
		m_rowsLbl->setParent(nullptr);
		delete m_rowsLbl;
		m_rowsLbl = nullptr;
	}
	if (m_colsLbl) {
		m_colsLbl->disconnect();
		m_colsLbl->setParent(nullptr);
		delete m_colsLbl;
		m_colsLbl = nullptr;
	}
	if (m_variantsLbl) {
		m_variantsLbl->disconnect();
		m_variantsLbl->setParent(nullptr);
		delete m_variantsLbl;
		m_variantsLbl = nullptr;
	}
	if (m_variantLbl) {
		m_variantLbl->disconnect();
		m_variantLbl->setParent(nullptr);
		delete m_variantLbl;
		m_variantLbl = nullptr;
	}
	if (m_checksLbl) {
		m_checksLbl->disconnect();
		m_checksLbl->setParent(nullptr);
		delete m_checksLbl;
		m_checksLbl = nullptr;
	}
	if (m_toolLbl) {
		m_toolLbl->disconnect();
		m_toolLbl->setParent(nullptr);
		delete m_toolLbl;
		m_toolLbl = nullptr;
	}
	if (m_fileButton) {
		m_fileButton->disconnect();
		m_fileButton->setParent(nullptr);
		delete m_fileButton;
		m_fileButton = nullptr;
	}
	if (m_fileMenu) {
		m_fileMenu->disconnect();
		m_fileMenu->setParent(nullptr);
		delete m_fileMenu;
		m_fileMenu = nullptr;
	}
	
	// Удаляем действия
	if (m_newAct) {
		m_newAct->disconnect();
		delete m_newAct;
		m_newAct = nullptr;
	}
	if (m_openAct) {
		m_openAct->disconnect();
		delete m_openAct;
		m_openAct = nullptr;
	}
	if (m_saveTaskAct) {
		m_saveTaskAct->disconnect();
		delete m_saveTaskAct;
		m_saveTaskAct = nullptr;
	}
	if (m_saveAct) {
		m_saveAct->disconnect();
		delete m_saveAct;
		m_saveAct = nullptr;
	}
	if (m_exportAct) {
		m_exportAct->disconnect();
		delete m_exportAct;
		m_exportAct = nullptr;
	}
	
	// Удаляем все виджеты из layout перед их удалением
	// Сначала удаляем toolbar и его содержимое, затем остальные виджеты
	if (layout()) {
		QLayout *oldLayout = layout();
		// Удаляем все виджеты из layout
		QLayoutItem *item;
		while ((item = oldLayout->takeAt(0)) != nullptr) {
			if (item->widget()) {
				QWidget *widget = item->widget();
				// Не удаляем toolbar здесь - он удаляется отдельно ниже
				if (widget != m_toolBar && widget != m_editor) {
					// Удаляем все дочерние виджеты виджета
					QList<QWidget*> children = widget->findChildren<QWidget*>();
					for (QWidget *child : children) {
						child->disconnect();
						child->setParent(nullptr);
						delete child;
					}
					widget->disconnect();
					widget->setParent(nullptr);
					delete widget;
				}
			}
			delete item;
		}
	}
	
	// Явно очищаем QToolBar и все его содержимое
	if (m_toolBar) {
		// Получаем все дочерние виджеты toolbar (включая виджеты строк инструментов)
		QList<QWidget*> toolbarWidgets = m_toolBar->findChildren<QWidget*>();
		
		// Также получаем виджеты через actions
		for (QAction *action : m_toolBar->actions()) {
			if (action->isSeparator()) {
				continue;
			}
			// В Qt 5.12 используем widgetForAction
			QWidget *widget = m_toolBar->widgetForAction(action);
			if (widget && !toolbarWidgets.contains(widget)) {
				toolbarWidgets.append(widget);
			}
		}
		
		// Удаляем все виджеты из toolbar
		// Сначала удаляем виджеты строк инструментов (они содержат кнопки)
		for (QWidget *widget : toolbarWidgets) {
			if (widget && widget->parent() == m_toolBar) {
				// Если это виджет строки инструментов, удаляем его layout и дочерние виджеты
				if (QHBoxLayout *layout = qobject_cast<QHBoxLayout*>(widget->layout())) {
					QLayoutItem *item;
					while ((item = layout->takeAt(0)) != nullptr) {
						if (item->widget()) {
							item->widget()->disconnect();
							item->widget()->setParent(nullptr);
							delete item->widget();
						}
						delete item;
					}
				}
				// Отключаем все связи виджета
				widget->disconnect();
				widget->setParent(nullptr);
				delete widget;
			}
		}
		
		// Удаляем все действия из toolbar
		m_toolBar->clear();
		// Отключаем все связи
		m_toolBar->disconnect();
		// Удаляем toolbar из layout, если он там есть
		if (layout()) {
			QLayoutItem *item;
			for (int i = 0; i < layout()->count(); ++i) {
				item = layout()->itemAt(i);
				if (item && item->widget() == m_toolBar) {
					layout()->removeItem(item);
					delete item;
					break;
				}
			}
		}
		// Удаляем toolbar
		m_toolBar->setParent(nullptr);
		delete m_toolBar;
		m_toolBar = nullptr;
	}
}

// Кастомный QComboBox с кастомной стрелкой для темной темы
class CustomComboBox : public QComboBox
{
public:
	CustomComboBox(QWidget *parent = nullptr) : QComboBox(parent), m_darkTheme(false) {}
	
	void setDarkTheme(bool dark) {
		m_darkTheme = dark;
		update();
	}
	
protected:
	void paintEvent(QPaintEvent *event) override {
		// Всегда рисуем стандартный виджет
		QComboBox::paintEvent(event);
		
		// Рисуем кастомную стрелку в обеих темах
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		
		// Получаем прямоугольник для кнопки выпадающего списка
		QStyleOptionComboBox option;
		initStyleOption(&option);
		QRect dropDownRect = style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
		
		if (dropDownRect.isValid()) {
			painter.save();
			
			// Цвет стрелки зависит от темы
			QColor arrowColor = m_darkTheme ? QColor(255, 255, 255) : QColor(0, 0, 0);
			QPen arrowPen(arrowColor);
			arrowPen.setWidth(1);
			painter.setPen(arrowPen);
			
			QPoint center = dropDownRect.center();
			int arrowSize = 3; // Такой же размер, как в CustomSpinBox
			QPoint points[3] = {
				QPoint(center.x(), center.y() + arrowSize),
				QPoint(center.x() - arrowSize, center.y() - arrowSize),
				QPoint(center.x() + arrowSize, center.y() - arrowSize)
			};
			painter.setBrush(arrowColor);
			painter.drawPolygon(points, 3);
			
			painter.restore();
		}
	}
	
private:
	bool m_darkTheme;
};

// Кастомный QSpinBox с цветными кнопками для темной темы
class CustomSpinBox : public QSpinBox
{
public:
	CustomSpinBox(QWidget *parent = nullptr) : QSpinBox(parent), m_darkTheme(false) {}
	
	void setDarkTheme(bool dark) {
		m_darkTheme = dark;
		update();
	}
	
protected:
	void paintEvent(QPaintEvent *event) override {
		// Всегда рисуем кастомные кнопки в обеих темах
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		
		// Получаем прямоугольники для кнопок
		QStyleOptionSpinBox option;
		initStyleOption(&option);
		QRect upRect = style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, this);
		QRect downRect = style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown, this);
		
		// Рисуем все части кроме кнопок
		QStyleOptionSpinBox copyOption = option;
		copyOption.subControls &= ~(QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown);
		style()->drawComplexControl(QStyle::CC_SpinBox, &copyOption, &painter, this);
		
		painter.save();
		
		// Цвет кнопок зависит от темы
		QColor buttonColor = m_darkTheme ? QColor(53, 53, 53) : QColor(240, 240, 240);
		QColor borderColor = m_darkTheme ? QColor(85, 85, 85) : QColor(204, 204, 204);
		QColor dividerColor = m_darkTheme ? QColor(85, 85, 85) : QColor(204, 204, 204);
		QColor arrowColor = m_darkTheme ? QColor(255, 255, 255) : QColor(0, 0, 0);
		
		// Рисуем верхнюю кнопку
		if (upRect.isValid()) {
			QColor currentButtonColor = buttonColor;
			if (option.state & QStyle::State_Sunken && (option.activeSubControls & QStyle::SC_SpinBoxUp)) {
				currentButtonColor = currentButtonColor.darker(120); // Немного темнее при нажатии
			}
			painter.fillRect(upRect, currentButtonColor);
			
			// Рисуем границу
			QPen borderPen(borderColor);
			borderPen.setWidth(1);
			painter.setPen(borderPen);
			painter.setBrush(Qt::NoBrush);
			painter.drawRect(upRect.adjusted(0, 0, -1, -1));
		}
		
		// Рисуем нижнюю кнопку
		if (downRect.isValid()) {
			QColor currentButtonColor = buttonColor;
			if (option.state & QStyle::State_Sunken && (option.activeSubControls & QStyle::SC_SpinBoxDown)) {
				currentButtonColor = currentButtonColor.darker(120); // Немного темнее при нажатии
			}
			painter.fillRect(downRect, currentButtonColor);
			
			// Рисуем границу
			QPen borderPen(borderColor);
			borderPen.setWidth(1);
			painter.setPen(borderPen);
			painter.setBrush(Qt::NoBrush);
			painter.drawRect(downRect.adjusted(0, 0, -1, -1));
		}
		
		// Рисуем разделительную линию между кнопками
		if (upRect.isValid() && downRect.isValid()) {
			QPen dividerPen(dividerColor);
			dividerPen.setWidth(1);
			painter.setPen(dividerPen);
			int dividerY = (upRect.bottom() + downRect.top()) / 2;
			painter.drawLine(upRect.left(), dividerY, upRect.right(), dividerY);
		}
		
		// Рисуем стрелки (цвет зависит от темы)
		QPen arrowPen(arrowColor);
		arrowPen.setWidth(1);
		painter.setPen(arrowPen);
		
		// Стрелка вверх
		if (upRect.isValid()) {
			QPoint center = upRect.center();
			int arrowSize = 3;
			QPoint points[3] = {
				QPoint(center.x(), center.y() - arrowSize),
				QPoint(center.x() - arrowSize, center.y() + arrowSize),
				QPoint(center.x() + arrowSize, center.y() + arrowSize)
			};
			painter.setBrush(arrowColor);
			painter.drawPolygon(points, 3);
		}
		
		// Стрелка вниз
		if (downRect.isValid()) {
			QPoint center = downRect.center();
			int arrowSize = 3;
			QPoint points[3] = {
				QPoint(center.x(), center.y() + arrowSize),
				QPoint(center.x() - arrowSize, center.y() - arrowSize),
				QPoint(center.x() + arrowSize, center.y() - arrowSize)
			};
			painter.setBrush(arrowColor);
			painter.drawPolygon(points, 3);
		}
		
		painter.restore();
	}
	
private:
	bool m_darkTheme;
};

// Кастомный стиль для отрисовки + и - вместо стрелок в QSpinBox
class SpinBoxPlusMinusStyle : public QProxyStyle
{
public:
	SpinBoxPlusMinusStyle(QStyle *baseStyle = nullptr) : QProxyStyle(baseStyle) {}
	
	void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
	{
		if (element == PE_IndicatorSpinUp || element == PE_IndicatorSpinDown) {
			// Рисуем + или - вместо стрелок
			painter->save();
			painter->setRenderHint(QPainter::Antialiasing, true);
			
			// Явно устанавливаем белый цвет для темной темы
			QPen pen(QColor(255, 255, 255));
			pen.setWidth(2);
			pen.setCapStyle(Qt::RoundCap);
			painter->setPen(pen);
			painter->setBrush(Qt::NoBrush);
			
			QRect rect = option->rect;
			QPoint center = rect.center();
			
			if (element == PE_IndicatorSpinUp) {
				// Рисуем +
				int size = qMin(rect.width(), rect.height()) / 3;
				// Горизонтальная линия
				painter->drawLine(center.x() - size, center.y(), center.x() + size, center.y());
				// Вертикальная линия
				painter->drawLine(center.x(), center.y() - size, center.x(), center.y() + size);
			} else {
				// Рисуем -
				int size = qMin(rect.width(), rect.height()) / 3;
				// Горизонтальная линия
				painter->drawLine(center.x() - size, center.y(), center.x() + size, center.y());
			}
			
			painter->restore();
		} else {
			QProxyStyle::drawPrimitive(element, option, painter, widget);
		}
	}
	
	void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override
	{
		if (control == CC_SpinBox) {
			const QStyleOptionSpinBox *spinBoxOption = qstyleoption_cast<const QStyleOptionSpinBox *>(option);
			if (spinBoxOption) {
				// Рисуем все части кроме кнопок
				QStyleOptionSpinBox copyOption = *spinBoxOption;
				copyOption.subControls &= ~(SC_SpinBoxUp | SC_SpinBoxDown);
				QProxyStyle::drawComplexControl(control, &copyOption, painter, widget);
				
				// Рисуем кнопки с + и -
				painter->save();
				painter->setRenderHint(QPainter::Antialiasing, true);
				
				// Получаем прямоугольники для кнопок
				QRect upRect = subControlRect(CC_SpinBox, spinBoxOption, SC_SpinBoxUp, widget);
				QRect downRect = subControlRect(CC_SpinBox, spinBoxOption, SC_SpinBoxDown, widget);
				
				// Рисуем фон кнопок
				if (upRect.isValid()) {
					QStyleOptionButton buttonOption;
					buttonOption.rect = upRect;
					buttonOption.state = spinBoxOption->state;
					if (spinBoxOption->activeSubControls & SC_SpinBoxUp) {
						buttonOption.state |= QStyle::State_Sunken;
					}
					drawControl(CE_PushButtonBevel, &buttonOption, painter, widget);
					
					// Рисуем + на верхней кнопке
					QPen pen(QColor(255, 255, 255));
					pen.setWidth(3);
					pen.setCapStyle(Qt::RoundCap);
					painter->setPen(pen);
					QPoint center = upRect.center();
					int size = qMin(upRect.width(), upRect.height()) / 3;
					painter->drawLine(center.x() - size, center.y(), center.x() + size, center.y());
					painter->drawLine(center.x(), center.y() - size, center.x(), center.y() + size);
				}
				
				if (downRect.isValid()) {
					QStyleOptionButton buttonOption;
					buttonOption.rect = downRect;
					buttonOption.state = spinBoxOption->state;
					if (spinBoxOption->activeSubControls & SC_SpinBoxDown) {
						buttonOption.state |= QStyle::State_Sunken;
					}
					drawControl(CE_PushButtonBevel, &buttonOption, painter, widget);
					
					// Рисуем - на нижней кнопке
					QPen pen(QColor(255, 255, 255));
					pen.setWidth(3);
					pen.setCapStyle(Qt::RoundCap);
					painter->setPen(pen);
					QPoint center = downRect.center();
					int size = qMin(downRect.width(), downRect.height()) / 3;
					painter->drawLine(center.x() - size, center.y(), center.x() + size, center.y());
				}
				
				painter->restore();
				return;
			}
		}
		QProxyStyle::drawComplexControl(control, option, painter, widget);
	}
};

void PyrobEditorWidget::setTheme(const QString &theme)
{
	m_theme = theme;
	
	// Применяем стили к виджетам
	QString styleSheet;
	if (theme == "dark") {
		styleSheet = 
			"QSpinBox, QComboBox { "
				"background-color: #232323; "
				"color: #ffffff; "
				"border: 1px solid #555555; "
				"padding: 2px; "
			"}"
			"QSpinBox:hover, QComboBox:hover { "
				"border: 1px solid #777777; "
			"}"
			"QSpinBox:focus, QComboBox:focus { "
				"border: 1px solid #2a82da; "
			"} "
			"QComboBox::drop-down { "
				"border: none; "
				"background-color: #353535; "
			"} "
			"QComboBox::down-arrow { "
				"image: none; "
				"border-left: 4px solid transparent; "
				"border-right: 4px solid transparent; "
				"border-top: 6px solid #ffffff; "
				"width: 0; "
				"height: 0; "
			"} "
			"QComboBox QAbstractItemView { "
				"background-color: #232323; "
				"color: #ffffff; "
				"selection-background-color: #2a82da; "
				"border: 1px solid #555555; "
			"}"
			"QLabel { "
				"color: #ffffff; "
			"}"
			"QToolBar { "
				"background-color: #353535; "
				"border: none; "
			"}"
			"QToolButton { "
				"background-color: transparent; "
				"color: #ffffff; "
				"border: 1px solid transparent; "
			"}"
			"QToolButton:hover { "
				"background-color: #555555; "
				"border: 1px solid #777777; "
			"}"
			"QToolButton:checked { "
				"background-color: #2a82da; "
				"border: 1px solid #2a82da; "
			"}"
			"QToolButton:checked:hover { "
				"background-color: #3a92ea; "
			"}"
			"QMenu { "
				"background-color: #353535; "
				"color: #ffffff; "
				"border: 1px solid #555555; "
			"}"
			"QMenu::item:selected { "
				"background-color: #555555; "
			"}";
	} else {
		styleSheet = 
			"QSpinBox, QComboBox { "
				"background-color: #ffffff; "
				"color: #000000; "
				"border: 1px solid #cccccc; "
				"padding: 2px; "
			"}"
			"QSpinBox:hover, QComboBox:hover { "
				"border: 1px solid #999999; "
			"}"
		"QSpinBox:focus, QComboBox:focus { "
			"border: 1px solid #2a82da; "
		"} "
		"QComboBox::drop-down { "
			"border: none; "
			"background-color: #f0f0f0; "
		"} "
		"QComboBox::down-arrow { "
			"image: none; "
			"border-left: 4px solid transparent; "
			"border-right: 4px solid transparent; "
			"border-top: 6px solid #000000; "
			"width: 0; "
			"height: 0; "
		"} "
			"QComboBox QAbstractItemView { "
				"background-color: #ffffff; "
				"color: #000000; "
				"selection-background-color: #2a82da; "
				"border: 1px solid #cccccc; "
			"}"
			"QLabel { "
				"color: #000000; "
			"}"
			"QToolBar { "
				"background-color: #f0f0f0; "
				"border: none; "
			"}"
			"QToolButton { "
				"background-color: transparent; "
				"color: #000000; "
				"border: 1px solid transparent; "
			"}"
			"QToolButton:hover { "
				"background-color: #e0e0e0; "
				"border: 1px solid #cccccc; "
			"}"
			"QToolButton:checked { "
				"background-color: #2a82da; "
				"border: 1px solid #2a82da; "
			"}"
			"QToolButton:checked:hover { "
				"background-color: #3a92ea; "
			"}"
			"QMenu { "
				"background-color: #ffffff; "
				"color: #000000; "
				"border: 1px solid #cccccc; "
			"}"
			"QMenu::item:selected { "
				"background-color: #e0e0e0; "
			"}";
	}
	
	// Разделяем стили для QSpinBox/QComboBox и остальных виджетов
	// Используем объектные имена с указанием родителя для максимальной специфичности
	QString spinComboStyle = theme == "dark" ?
		"#pyrobEditorWidget #pyrobRowsSpin, #pyrobEditorWidget #pyrobColsSpin, #pyrobEditorWidget #pyrobVariantsSpin, #pyrobEditorWidget #pyrobChecksSpin, #pyrobEditorWidget #pyrobVariantCombo { "
			"background-color: #232323; "
			"color: #ffffff; "
			"border: 1px solid #555555; "
			"padding: 2px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin:hover, #pyrobEditorWidget #pyrobColsSpin:hover, #pyrobEditorWidget #pyrobVariantsSpin:hover, #pyrobEditorWidget #pyrobChecksSpin:hover, #pyrobEditorWidget #pyrobVariantCombo:hover { "
			"border: 1px solid #777777; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin:focus, #pyrobEditorWidget #pyrobColsSpin:focus, #pyrobEditorWidget #pyrobVariantsSpin:focus, #pyrobEditorWidget #pyrobChecksSpin:focus, #pyrobEditorWidget #pyrobVariantCombo:focus { "
			"border: 1px solid #2a82da; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::up-button, #pyrobEditorWidget #pyrobColsSpin::up-button, #pyrobEditorWidget #pyrobVariantsSpin::up-button, #pyrobEditorWidget #pyrobChecksSpin::up-button { "
			"background-color: #353535; "
			"border: 1px solid #555555; "
			"border-top-right-radius: 3px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::up-button:hover, #pyrobEditorWidget #pyrobColsSpin::up-button:hover, #pyrobEditorWidget #pyrobVariantsSpin::up-button:hover, #pyrobEditorWidget #pyrobChecksSpin::up-button:hover { "
			"background-color: #454545; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::up-arrow, #pyrobEditorWidget #pyrobColsSpin::up-arrow, #pyrobEditorWidget #pyrobVariantsSpin::up-arrow, #pyrobEditorWidget #pyrobChecksSpin::up-arrow { "
			"image: none; "
			"width: 0px; "
			"height: 0px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::down-button, #pyrobEditorWidget #pyrobColsSpin::down-button, #pyrobEditorWidget #pyrobVariantsSpin::down-button, #pyrobEditorWidget #pyrobChecksSpin::down-button { "
			"background-color: #353535; "
			"border: 1px solid #555555; "
			"border-bottom-right-radius: 3px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::down-button:hover, #pyrobEditorWidget #pyrobColsSpin::down-button:hover, #pyrobEditorWidget #pyrobVariantsSpin::down-button:hover, #pyrobEditorWidget #pyrobChecksSpin::down-button:hover { "
			"background-color: #454545; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::down-arrow, #pyrobEditorWidget #pyrobColsSpin::down-arrow, #pyrobEditorWidget #pyrobVariantsSpin::down-arrow, #pyrobEditorWidget #pyrobChecksSpin::down-arrow { "
			"image: none; "
			"width: 0px; "
			"height: 0px; "
		"}"
		"#pyrobEditorWidget #pyrobVariantCombo::drop-down { "
			"border: none; "
			"background-color: #353535; "
			"width: 20px; "
		"}"
		"#pyrobEditorWidget #pyrobVariantCombo::down-arrow { "
			"image: none; "
			"border-left: 4px solid transparent; "
			"border-right: 4px solid transparent; "
			"border-top: 6px solid #ffffff; "
			"width: 0; "
			"height: 0; "
		"}"
		"#pyrobEditorWidget #pyrobVariantCombo QAbstractItemView { "
			"background-color: #232323; "
			"color: #ffffff; "
			"selection-background-color: #2a82da; "
			"border: 1px solid #555555; "
		"}" :
		"#pyrobEditorWidget #pyrobRowsSpin, #pyrobEditorWidget #pyrobColsSpin, #pyrobEditorWidget #pyrobVariantsSpin, #pyrobEditorWidget #pyrobChecksSpin, #pyrobEditorWidget #pyrobVariantCombo { "
			"background-color: #ffffff; "
			"color: #000000; "
			"border: 1px solid #cccccc; "
			"padding: 2px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin:hover, #pyrobEditorWidget #pyrobColsSpin:hover, #pyrobEditorWidget #pyrobVariantsSpin:hover, #pyrobEditorWidget #pyrobChecksSpin:hover, #pyrobEditorWidget #pyrobVariantCombo:hover { "
			"border: 1px solid #999999; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin:focus, #pyrobEditorWidget #pyrobColsSpin:focus, #pyrobEditorWidget #pyrobVariantsSpin:focus, #pyrobEditorWidget #pyrobChecksSpin:focus, #pyrobEditorWidget #pyrobVariantCombo:focus { "
			"border: 1px solid #2a82da; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::up-button, #pyrobEditorWidget #pyrobColsSpin::up-button, #pyrobEditorWidget #pyrobVariantsSpin::up-button, #pyrobEditorWidget #pyrobChecksSpin::up-button { "
			"background-color: #f0f0f0; "
			"border: 1px solid #cccccc; "
			"border-top-right-radius: 3px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::up-button:hover, #pyrobEditorWidget #pyrobColsSpin::up-button:hover, #pyrobEditorWidget #pyrobVariantsSpin::up-button:hover, #pyrobEditorWidget #pyrobChecksSpin::up-button:hover { "
			"background-color: #e0e0e0; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::up-arrow, #pyrobEditorWidget #pyrobColsSpin::up-arrow, #pyrobEditorWidget #pyrobVariantsSpin::up-arrow, #pyrobEditorWidget #pyrobChecksSpin::up-arrow { "
			"image: none; "
			"border-left: 4px solid transparent; "
			"border-right: 4px solid transparent; "
			"border-bottom: 6px solid #000000; "
			"width: 0; "
			"height: 0; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::down-button, #pyrobEditorWidget #pyrobColsSpin::down-button, #pyrobEditorWidget #pyrobVariantsSpin::down-button, #pyrobEditorWidget #pyrobChecksSpin::down-button { "
			"background-color: #f0f0f0; "
			"border: 1px solid #cccccc; "
			"border-bottom-right-radius: 3px; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::down-button:hover, #pyrobEditorWidget #pyrobColsSpin::down-button:hover, #pyrobEditorWidget #pyrobVariantsSpin::down-button:hover, #pyrobEditorWidget #pyrobChecksSpin::down-button:hover { "
			"background-color: #e0e0e0; "
		"}"
		"#pyrobEditorWidget #pyrobRowsSpin::down-arrow, #pyrobEditorWidget #pyrobColsSpin::down-arrow, #pyrobEditorWidget #pyrobVariantsSpin::down-arrow, #pyrobEditorWidget #pyrobChecksSpin::down-arrow { "
			"image: none; "
			"border-left: 4px solid transparent; "
			"border-right: 4px solid transparent; "
			"border-top: 6px solid #000000; "
			"width: 0; "
			"height: 0; "
		"}"
		"#pyrobEditorWidget #pyrobVariantCombo::drop-down { "
			"border: none; "
			"background-color: #f0f0f0; "
			"width: 20px; "
		"}"
		"#pyrobEditorWidget #pyrobVariantCombo::down-arrow { "
			"image: none; "
			"border-left: 4px solid transparent; "
			"border-right: 4px solid transparent; "
			"border-top: 6px solid #000000; "
			"width: 0; "
			"height: 0; "
		"}"
		"#pyrobEditorWidget #pyrobVariantCombo QAbstractItemView { "
			"background-color: #ffffff; "
			"color: #000000; "
			"selection-background-color: #2a82da; "
			"border: 1px solid #cccccc; "
		"}";
	
	// Применяем стили и палитру к конкретным виджетам с принудительным обновлением
	QPalette spinComboPalette;
	if (theme == "dark") {
		spinComboPalette.setColor(QPalette::Base, QColor(35, 35, 35));
		spinComboPalette.setColor(QPalette::Text, QColor(255, 255, 255));
		spinComboPalette.setColor(QPalette::Button, QColor(53, 53, 53));
		spinComboPalette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
	} else {
		spinComboPalette.setColor(QPalette::Base, QColor(255, 255, 255));
		spinComboPalette.setColor(QPalette::Text, QColor(0, 0, 0));
		spinComboPalette.setColor(QPalette::Button, QColor(240, 240, 240));
		spinComboPalette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
	}
	
	// Применяем стили через родительский виджет для максимального приоритета
	// Это гарантирует, что стили не будут перезаписаны глобальными стилями
	setStyleSheet(spinComboStyle);
	
	// Принудительно обновляем стили виджетов через unpolish/polish
	QStyle *style = QApplication::style();
	
	// Также применяем палитру и стили к конкретным виджетам
	if (m_rowsSpin) {
		style->unpolish(m_rowsSpin);
		m_rowsSpin->setPalette(spinComboPalette);
		m_rowsSpin->setAutoFillBackground(true);
		style->polish(m_rowsSpin);
		m_rowsSpin->update();
		m_rowsSpin->repaint();
	}
	if (m_colsSpin) {
		style->unpolish(m_colsSpin);
		m_colsSpin->setPalette(spinComboPalette);
		m_colsSpin->setAutoFillBackground(true);
		style->polish(m_colsSpin);
		m_colsSpin->update();
		m_colsSpin->repaint();
	}
	if (m_variantsSpin) {
		style->unpolish(m_variantsSpin);
		m_variantsSpin->setPalette(spinComboPalette);
		m_variantsSpin->setAutoFillBackground(true);
		style->polish(m_variantsSpin);
		m_variantsSpin->update();
		m_variantsSpin->repaint();
	}
	if (m_checksSpin) {
		style->unpolish(m_checksSpin);
		m_checksSpin->setPalette(spinComboPalette);
		m_checksSpin->setAutoFillBackground(true);
		style->polish(m_checksSpin);
		m_checksSpin->update();
		m_checksSpin->repaint();
	}
	if (m_variantCombo) {
		style->unpolish(m_variantCombo);
		m_variantCombo->setPalette(spinComboPalette);
		m_variantCombo->setAutoFillBackground(true);
		style->polish(m_variantCombo);
		m_variantCombo->update();
		m_variantCombo->repaint();
	}
	
	// Применяем темную тему к кастомным QSpinBox
	if (m_rowsSpin) {
		CustomSpinBox *customSpin = dynamic_cast<CustomSpinBox*>(m_rowsSpin);
		if (customSpin) {
			customSpin->setDarkTheme(theme == "dark");
		}
	}
	if (m_colsSpin) {
		CustomSpinBox *customSpin = dynamic_cast<CustomSpinBox*>(m_colsSpin);
		if (customSpin) {
			customSpin->setDarkTheme(theme == "dark");
		}
	}
	if (m_variantsSpin) {
		CustomSpinBox *customSpin = dynamic_cast<CustomSpinBox*>(m_variantsSpin);
		if (customSpin) {
			customSpin->setDarkTheme(theme == "dark");
		}
	}
	if (m_checksSpin) {
		CustomSpinBox *customSpin = dynamic_cast<CustomSpinBox*>(m_checksSpin);
		if (customSpin) {
			customSpin->setDarkTheme(theme == "dark");
		}
	}
	
	// Применяем темную тему к кастомному QComboBox
	if (m_variantCombo) {
		CustomComboBox *customCombo = dynamic_cast<CustomComboBox*>(m_variantCombo);
		if (customCombo) {
			customCombo->setDarkTheme(theme == "dark");
		}
	}
	
	// Отложенное применение стилей через QTimer для гарантии применения после глобальных стилей
	QTimer::singleShot(0, this, [this, spinComboStyle, spinComboPalette]() {
		setStyleSheet(spinComboStyle);
		if (m_rowsSpin) {
			m_rowsSpin->setPalette(spinComboPalette);
			m_rowsSpin->update();
		}
		if (m_colsSpin) {
			m_colsSpin->setPalette(spinComboPalette);
			m_colsSpin->update();
		}
		if (m_variantsSpin) {
			m_variantsSpin->setPalette(spinComboPalette);
			m_variantsSpin->update();
		}
		if (m_checksSpin) {
			m_checksSpin->setPalette(spinComboPalette);
			m_checksSpin->update();
		}
		if (m_variantCombo) {
			m_variantCombo->setPalette(spinComboPalette);
			m_variantCombo->update();
		}
	});
	
	// Применяем стили к остальным виджетам
	if (m_rowsLbl) {
		m_rowsLbl->setStyleSheet(styleSheet);
		m_rowsLbl->update();
	}
	if (m_colsLbl) {
		m_colsLbl->setStyleSheet(styleSheet);
		m_colsLbl->update();
	}
	if (m_variantsLbl) {
		m_variantsLbl->setStyleSheet(styleSheet);
		m_variantsLbl->update();
	}
	if (m_variantLbl) {
		m_variantLbl->setStyleSheet(styleSheet);
		m_variantLbl->update();
	}
	if (m_checksLbl) {
		m_checksLbl->setStyleSheet(styleSheet);
		m_checksLbl->update();
	}
	if (m_toolLbl) {
		m_toolLbl->setStyleSheet(styleSheet);
		m_toolLbl->update();
	}
	if (m_toolBar) {
		m_toolBar->setStyleSheet(styleSheet);
		m_toolBar->update();
	}
	if (m_fileButton) {
		m_fileButton->setStyleSheet(styleSheet);
		m_fileButton->update();
	}
	if (m_fileMenu) {
		m_fileMenu->setStyleSheet(styleSheet);
		m_fileMenu->update();
	}
	
	// Применяем стили ко всем кнопкам инструментов
	if (m_toolGroup) {
		QList<QAbstractButton*> buttons = m_toolGroup->buttons();
		for (QAbstractButton* btn : buttons) {
			btn->setStyleSheet(styleSheet);
			btn->update();
		}
	}
	
	// Обновляем тему редактора сетки
	if (m_editor) {
		m_editor->setTheme(theme);
	}
	
	// Принудительно обновляем весь виджет
	update();
	repaint();
}

void PyrobEditorWidget::createUi()
{
	setObjectName("pyrobEditorWidget"); // Устанавливаем объектное имя для специфичных селекторов
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    createActions();
	createToolbar();
	
	// Панель инструментов слева (вертикальная)
	layout->addWidget(m_toolBar);
	// Редактор справа
	layout->addWidget(m_editor, 1);
	
	setLayout(layout);
}

void PyrobEditorWidget::createActions()
{
	m_newAct = new QAction(tr("Новый"), this);
	connect(m_newAct, &QAction::triggered, this, &PyrobEditorWidget::newProject);

	m_openAct = new QAction(tr("Открыть..."), this);
	connect(m_openAct, &QAction::triggered, this, &PyrobEditorWidget::openProject);

    m_saveTaskAct = new QAction(tr("Сохранить задачу (.py)"), this);
    connect(m_saveTaskAct, &QAction::triggered, this, &PyrobEditorWidget::saveTask);

    m_saveAct = new QAction(tr("Сохранить проект (.json)"), this);
    connect(m_saveAct, &QAction::triggered, this, &PyrobEditorWidget::saveProject);

	m_exportAct = new QAction(tr("Экспорт в Python..."), this);
	connect(m_exportAct, &QAction::triggered, this, &PyrobEditorWidget::exportPython);
}

void PyrobEditorWidget::createToolbar()
{
	m_toolBar = new QToolBar(tr("Основное"), this);
	m_toolBar->setOrientation(Qt::Vertical);
	m_toolBar->setMovable(false);
	m_toolBar->setMaximumWidth(66);
	m_toolBar->setMinimumWidth(66);
    // File drop-down button inside toolbar
    m_fileMenu = new QMenu(this);
    m_fileMenu->setTitle(trKey("file"));
    m_fileMenu->addAction(m_newAct);
    m_fileMenu->addAction(m_openAct);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveTaskAct);
    m_fileMenu->addAction(m_saveAct);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exportAct);

    m_fileButton = new QToolButton(this);
    m_fileButton->setText(trKey("file"));
    m_fileButton->setMenu(m_fileMenu);
    m_fileButton->setPopupMode(QToolButton::InstantPopup);
    m_fileButton->setMaximumWidth(85);
    m_toolBar->addWidget(m_fileButton);
    m_toolBar->addSeparator();

	m_rowsSpin = new CustomSpinBox(this);
	m_rowsSpin->setObjectName("pyrobRowsSpin");
	m_rowsSpin->setRange(1, 200);
	m_rowsSpin->setValue(10);
	m_rowsSpin->setMaximumWidth(60);
	connect(m_rowsSpin, SIGNAL(valueChanged(int)), this, SLOT(gridSizeChanged()));

	m_colsSpin = new CustomSpinBox(this);
	m_colsSpin->setObjectName("pyrobColsSpin");
	m_colsSpin->setRange(1, 200);
	m_colsSpin->setValue(10);
	m_colsSpin->setMaximumWidth(60);
	connect(m_colsSpin, SIGNAL(valueChanged(int)), this, SLOT(gridSizeChanged()));

    m_rowsLbl = new QLabel(this);
    m_toolBar->addWidget(m_rowsLbl);
    m_toolBar->addWidget(m_rowsSpin);
    m_colsLbl = new QLabel(this);
    m_toolBar->addWidget(m_colsLbl);
    m_toolBar->addWidget(m_colsSpin);

    m_variantsSpin = new CustomSpinBox(this);
    m_variantsSpin->setObjectName("pyrobVariantsSpin");
    m_variantsSpin->setRange(1, 100);
    m_variantsSpin->setValue(1);
    m_variantsSpin->setMaximumWidth(60);
    connect(m_variantsSpin, SIGNAL(valueChanged(int)), this, SLOT(variantsCountChanged(int)));
    m_variantsLbl = new QLabel(this);
    m_toolBar->addWidget(m_variantsLbl);
    m_toolBar->addWidget(m_variantsSpin);

    m_variantCombo = new CustomComboBox(this);
    m_variantCombo->setObjectName("pyrobVariantCombo");
    m_variantCombo->addItem("");
    m_variantCombo->setMaximumWidth(60);
    connect(m_variantCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(currentVariantChanged(int)));
    m_variantLbl = new QLabel(this);
    m_toolBar->addWidget(m_variantLbl);
    m_toolBar->addWidget(m_variantCombo);

    m_checksSpin = new CustomSpinBox(this);
    m_checksSpin->setObjectName("pyrobChecksSpin");
    m_checksSpin->setRange(1, 1);
    m_checksSpin->setValue(1);
    m_checksSpin->setMaximumWidth(60);
    connect(m_checksSpin, SIGNAL(valueChanged(int)), this, SLOT(checksChanged(int)));
    m_checksLbl = new QLabel(this);
    m_toolBar->addWidget(m_checksLbl);
    m_toolBar->addWidget(m_checksSpin);

    // Tool buttons with icons
    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);

    auto makeIcon = [](QColor fill, bool ring, bool dot, bool wall)->QIcon{
        QPixmap pm(24, 24); pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        QRect r(3,3,18,18);
        if (fill.isValid()) { p.fillRect(r, fill); p.setPen(QPen(Qt::gray,1)); p.drawRect(r); }
        if (ring) { p.setPen(QPen(Qt::blue,2)); p.drawEllipse(r.adjusted(4,4,-4,-4)); }
        if (dot) { p.setBrush(Qt::black); p.setPen(Qt::NoPen); p.drawEllipse(QRect(r.center().x()-3, r.center().y()-3, 6, 6)); }
        if (wall) { p.setPen(QPen(QColor(120,120,120),3)); p.drawLine(r.right(), r.top(), r.right(), r.bottom()); }
        return QIcon(pm);
    };

    // Метка "Инструмент:" перед кнопками
    m_toolBar->addSeparator();
    m_toolLbl = new QLabel(this);
    m_toolBar->addWidget(m_toolLbl);

    struct ToolDef { int id; QColor fill; bool ring; bool dot; bool wall; const char* tipKey; };
    QVector<ToolDef> defsRow1 = {
        { (int)GridEditor::PaintToBeFilled, QColor(161,220,216), false, false, false, "tool_tbf" },
        { (int)GridEditor::PaintFilled, QColor(255,248,209), false, false, false, "tool_filled" }
    };
    QVector<ToolDef> defsRow2 = {
        { (int)GridEditor::PaintEmpty, QColor(255,255,255), false, false, false, "tool_empty" },
        { (int)GridEditor::ToggleWall, QColor(), false, false, true, "tool_wall" }
    };
    QVector<ToolDef> defsRow3 = {
        { (int)GridEditor::SetStart, QColor(), true, false, false, "tool_start" },
        { (int)GridEditor::SetParking, QColor(), false, true, false, "tool_parking" }
    };

    // Первая строка: закрасить, закрашена
    QWidget *toolRow1 = new QWidget(this);
    QHBoxLayout *row1Layout = new QHBoxLayout(toolRow1);
    row1Layout->setContentsMargins(0, 0, 0, 0);
    row1Layout->setSpacing(2);
    for (const auto &d : defsRow1) {
        QToolButton *btn = new QToolButton(this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setIcon(makeIcon(d.fill, d.ring, d.dot, d.wall));
        btn->setToolTip(trKey(d.tipKey));
        btn->setFixedSize(28, 28);
        btn->setIconSize(QSize(24,24));
        row1Layout->addWidget(btn);
        m_toolGroup->addButton(btn, d.id);
    }
    m_toolBar->addWidget(toolRow1);

    // Вторая строка: пустая, стена
    QWidget *toolRow2 = new QWidget(this);
    QHBoxLayout *row2Layout = new QHBoxLayout(toolRow2);
    row2Layout->setContentsMargins(0, 0, 0, 0);
    row2Layout->setSpacing(2);
    for (const auto &d : defsRow2) {
        QToolButton *btn = new QToolButton(this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setIcon(makeIcon(d.fill, d.ring, d.dot, d.wall));
        btn->setToolTip(trKey(d.tipKey));
        btn->setFixedSize(28, 28);
        btn->setIconSize(QSize(24,24));
        row2Layout->addWidget(btn);
        m_toolGroup->addButton(btn, d.id);
    }
    m_toolBar->addWidget(toolRow2);

    // Третья строка: старт, парковка
    QWidget *toolRow3 = new QWidget(this);
    QHBoxLayout *row3Layout = new QHBoxLayout(toolRow3);
    row3Layout->setContentsMargins(0, 0, 0, 0);
    row3Layout->setSpacing(2);
    for (const auto &d : defsRow3) {
        QToolButton *btn = new QToolButton(this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setIcon(makeIcon(d.fill, d.ring, d.dot, d.wall));
        btn->setToolTip(trKey(d.tipKey));
        btn->setFixedSize(28, 28);
        btn->setIconSize(QSize(24,24));
        row3Layout->addWidget(btn);
        m_toolGroup->addButton(btn, d.id);
    }
    m_toolBar->addWidget(toolRow3);
    
    connect(m_toolGroup, SIGNAL(buttonClicked(int)), this, SLOT(toolSelected(int)));
    if (auto *b = m_toolGroup->button((int)GridEditor::PaintEmpty)) { b->setChecked(true); }
    toolSelected((int)GridEditor::PaintEmpty);

	// Initialize grid size
    m_model->resizeGrid(m_rowsSpin->value(), m_colsSpin->value());
    m_model->setVariantCount(m_variantsSpin->value());
    m_model->setChecks(m_checksSpin->value());
    retranslateUi();
}

void PyrobEditorWidget::newProject()
{
	if (!maybeSave()) return;
	m_model->resizeGrid(10, 10);
	m_model->clearStart();
	m_model->clearParking();
	m_rowsSpin->setValue(10);
	m_colsSpin->setValue(10);
	m_currentProjectPath.clear();
    m_currentTaskPath.clear();
}

QString PyrobEditorWidget::defaultTasksDirectory(bool forSaveOrExport) const
{
	QString pythonPath;
	if (m_mainWindow) {
		pythonPath = m_mainWindow->pythonPath();
	}

	bool pyrobInstalled = false;
	const QString tasksDir = PythonInterpreterPaths::pyrobTasksDir(pythonPath, &pyrobInstalled);
	if (pyrobInstalled) {
		return tasksDir;
	}

	if (forSaveOrExport) {
		return documentsDirectory();
	}

	const QString embeddedTasks = QDir(QCoreApplication::applicationDirPath())
		.filePath("Python38/Lib/site-packages/pyrob/tasks");
	if (QDir(embeddedTasks).exists()) {
		return embeddedTasks;
	}

	return documentsDirectory();
}

void PyrobEditorWidget::openProject()
{
	if (!maybeSave()) return;
	QString path = showFilePicker(trKey("project_filter"), defaultTasksDirectory(), false, false);
	if (path.isEmpty()) return;
	if (loadFromFile(path)) {
		m_currentProjectPath = path;
	}
}

void PyrobEditorWidget::saveProject()
{
	QString path = m_currentProjectPath;
	if (path.isEmpty()) {
        path = showFilePicker(trKey("project_filter"), defaultTasksDirectory(true), true, false);
		if (path.isEmpty()) return;
		if (!path.endsWith(".pyrob.json")) path += ".pyrob.json";
	}
	if (saveToFile(path)) {
		m_currentProjectPath = path;
	}
}

void PyrobEditorWidget::exportPython()
{
    QString path = showFilePicker(trKey("python_filter"), defaultTasksDirectory(true), true, false);
	if (path.isEmpty()) return;
	if (!path.endsWith(".py")) path += ".py";
	QString baseName = QFileInfo(path).baseName();
	QString py = m_model->generatePythonTask(baseName);
	QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, trKey("error"), trKey("cannot_write"));
		return;
	}
	f.write(py.toUtf8());
	f.close();
    m_currentTaskPath = path;
	QMessageBox::information(this, tr("Экспорт"), tr("Задача Python экспортирована."));
}

void PyrobEditorWidget::saveTask()
{
    QString path = m_currentTaskPath;
    if (path.isEmpty()) {
        QString defaultDir = defaultTasksDirectory(true);
        QString suggested = QDir(defaultDir).filePath("task_1_1.py");
        path = showFilePicker(trKey("python_filter"), suggested, true, false);
        if (path.isEmpty()) return;
        if (!path.endsWith(".py")) path += ".py";
    }
    QString baseName = QFileInfo(path).baseName();
    QString py = m_model->generatePythonTask(baseName);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, trKey("error"), trKey("cannot_write"));
        return;
    }
    f.write(py.toUtf8());
    f.close();
    m_currentTaskPath = path;
}

void PyrobEditorWidget::gridSizeChanged()
{
	m_model->resizeGrid(m_rowsSpin->value(), m_colsSpin->value());
}

void PyrobEditorWidget::setToolIndex(int index)
{
	m_editor->setTool(static_cast<GridEditor::Tool>(index));
}

void PyrobEditorWidget::toolSelected(int id)
{
    setToolIndex(id);
}

void PyrobEditorWidget::variantsCountChanged(int value)
{
    int old = m_model->variantCount();
    m_model->setVariantCount(value);
    m_checksSpin->setMaximum(value);
    if (m_checksSpin->value() > value) m_checksSpin->setValue(value);
    m_model->setChecks(m_checksSpin->value());
    // rebuild combo items
    m_variantCombo->blockSignals(true);
    m_variantCombo->clear();
    for (int i = 0; i < m_model->variantCount(); ++i) m_variantCombo->addItem(QString::number(i+1));
    int keep = qMin(old-1, m_model->currentVariantIndex());
    if (keep < 0) keep = 0;
    m_variantCombo->setCurrentIndex(keep);
    m_variantCombo->blockSignals(false);
}

void PyrobEditorWidget::currentVariantChanged(int index)
{
    m_model->setCurrentVariantIndex(index);
}

void PyrobEditorWidget::checksChanged(int value)
{
    Q_UNUSED(value);
    // clamp in model
    m_model->setChecks(m_checksSpin->value());
}

bool PyrobEditorWidget::maybeSave()
{
	// Упрощенная версия - всегда разрешаем продолжение
	// Можно добавить проверку изменений, если нужно
	return true;
}

bool PyrobEditorWidget::saveToFile(const QString &path)
{
	QJsonObject obj;
	m_model->toJson(obj);
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
	QJsonDocument doc(obj);
	f.write(doc.toJson());
	f.close();
	return true;
}

bool PyrobEditorWidget::loadFromFile(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return false;
	QByteArray data = f.readAll();
	f.close();
	QJsonDocument doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) return false;
	
	// Блокируем сигналы спинбоксов, чтобы не вызвать resizeGrid при установке значений
	// Это предотвратит очистку загруженных данных
	m_rowsSpin->blockSignals(true);
	m_colsSpin->blockSignals(true);
	m_variantsSpin->blockSignals(true);
	m_checksSpin->blockSignals(true);
	
	bool ok = m_model->fromJson(doc.object());
	if (ok) {
		// Устанавливаем значения спинбоксов (сигналы заблокированы, resizeGrid не вызовется)
		m_rowsSpin->setValue(m_model->rows());
		m_colsSpin->setValue(m_model->cols());
        m_variantsSpin->setValue(m_model->variantCount());
        m_checksSpin->setMaximum(m_model->variantCount());
        m_checksSpin->setValue(m_model->checks());
        // rebuild variant combo
        m_variantCombo->blockSignals(true);
        m_variantCombo->clear();
        for (int i = 0; i < m_model->variantCount(); ++i) m_variantCombo->addItem(QString::number(i+1));
        m_variantCombo->setCurrentIndex(m_model->currentVariantIndex());
        m_variantCombo->blockSignals(false);
	}
	
	// Разблокируем сигналы после установки всех значений
	m_rowsSpin->blockSignals(false);
	m_colsSpin->blockSignals(false);
	m_variantsSpin->blockSignals(false);
	m_checksSpin->blockSignals(false);
	
	return ok;
}

QString PyrobEditorWidget::showFilePicker(const QString &filter, const QString &initialDir, bool isSave, bool isDirectory)
{
	if (m_mainWindow) {
		return m_mainWindow->showFilePicker(filter, initialDir, isSave, isDirectory);
	}
	
	// Если указатель не сохранен, ищем MainWindow через цепочку родителей
	QWidget *p = parentWidget();
	while (p) {
		MainWindow *mainWindow = qobject_cast<MainWindow*>(p);
		if (mainWindow) {
			m_mainWindow = mainWindow; // Сохраняем для будущих вызовов
			return mainWindow->showFilePicker(filter, initialDir, isSave, isDirectory);
		}
		p = p->parentWidget();
	}
	
	// Если не нашли через родителей, ищем среди всех top-level виджетов
	QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
	for (QWidget *widget : topLevelWidgets) {
		MainWindow *mainWindow = qobject_cast<MainWindow*>(widget);
		if (mainWindow) {
			m_mainWindow = mainWindow; // Сохраняем для будущих вызовов
			return mainWindow->showFilePicker(filter, initialDir, isSave, isDirectory);
		}
	}
	
	return QString();
}

QString PyrobEditorWidget::trKey(const char *key) const
{
	// Всегда возвращаем русские строки
    if (strcmp(key, "file") == 0) return QString::fromUtf8("Файл");
	if (strcmp(key, "open_project") == 0) return QString::fromUtf8("Открыть проект");
	if (strcmp(key, "save_project") == 0) return QString::fromUtf8("Сохранить проект");
	if (strcmp(key, "project_filter") == 0) return QString::fromUtf8("Pyrob проект (*.pyrob.json)");
	if (strcmp(key, "export_python") == 0) return QString::fromUtf8("Экспортировать задачу");
	if (strcmp(key, "python_filter") == 0) return QString::fromUtf8("Python файл (*.py)");
	if (strcmp(key, "save_task") == 0) return QString::fromUtf8("Сохранить задачу");
	if (strcmp(key, "error") == 0) return QString::fromUtf8("Ошибка");
	if (strcmp(key, "cannot_write") == 0) return QString::fromUtf8("Не удалось записать файл");
	if (strcmp(key, "unsaved") == 0) return QString::fromUtf8("Есть несохранённые изменения");
	if (strcmp(key, "save_changes") == 0) return QString::fromUtf8("Сохранить изменения?");
	if (strcmp(key, "title") == 0) return QString::fromUtf8("Pyrob Конструктор задач[*]");
	if (strcmp(key, "new") == 0) return QString::fromUtf8("Новый");
	if (strcmp(key, "open") == 0) return QString::fromUtf8("Открыть...");
	if (strcmp(key, "save_task_act") == 0) return QString::fromUtf8("Сохранить задачу (.py)");
	if (strcmp(key, "save_project_act") == 0) return QString::fromUtf8("Сохранить проект (.json)");
	if (strcmp(key, "export") == 0) return QString::fromUtf8("Экспорт в Python...");
	if (strcmp(key, "rows") == 0) return QString::fromUtf8("Строки:");
	if (strcmp(key, "cols") == 0) return QString::fromUtf8("Столбцы:");
	if (strcmp(key, "variants") == 0) return QString::fromUtf8("Варианты:");
	if (strcmp(key, "variant") == 0) return QString::fromUtf8("Вариант:");
	if (strcmp(key, "checks") == 0) return QString::fromUtf8("Проверок:");
	if (strcmp(key, "tool") == 0) return QString::fromUtf8("Инструмент:");
	if (strcmp(key, "tool_empty") == 0) return QString::fromUtf8("Пустая");
	if (strcmp(key, "tool_tbf") == 0) return QString::fromUtf8("Закрасить");
	if (strcmp(key, "tool_filled") == 0) return QString::fromUtf8("Закрашена");
	if (strcmp(key, "tool_start") == 0) return QString::fromUtf8("Старт");
	if (strcmp(key, "tool_parking") == 0) return QString::fromUtf8("Парковка");
	if (strcmp(key, "tool_wall") == 0) return QString::fromUtf8("Стена");
	if (strcmp(key, "variant_name") == 0) return QString::fromUtf8("Вариант %1");
	return QString();
}

void PyrobEditorWidget::retranslateUi()
{
	if (m_newAct) m_newAct->setText(trKey("new"));
	if (m_openAct) m_openAct->setText(trKey("open"));
	if (m_saveTaskAct) m_saveTaskAct->setText(trKey("save_task_act"));
	if (m_saveAct) m_saveAct->setText(trKey("save_project_act"));
	if (m_exportAct) m_exportAct->setText(trKey("export"));
	if (m_fileMenu) m_fileMenu->setTitle(trKey("file"));
	if (m_fileButton) m_fileButton->setText(trKey("file"));
	if (m_rowsLbl) m_rowsLbl->setText(trKey("rows"));
	if (m_colsLbl) m_colsLbl->setText(trKey("cols"));
	if (m_variantsLbl) m_variantsLbl->setText(trKey("variants"));
	if (m_variantLbl) m_variantLbl->setText(trKey("variant"));
	if (m_checksLbl) m_checksLbl->setText(trKey("checks"));
	if (m_toolLbl) m_toolLbl->setText(trKey("tool"));
    if (m_toolGroup) {
        for (auto *btn : m_toolGroup->buttons()) {
            int id = m_toolGroup->id(btn);
            const char* key = id==GridEditor::PaintEmpty?"tool_empty":
                              id==GridEditor::PaintToBeFilled?"tool_tbf":
                              id==GridEditor::PaintFilled?"tool_filled":
                              id==GridEditor::SetStart?"tool_start":
                              id==GridEditor::SetParking?"tool_parking":"tool_wall";
            btn->setToolTip(trKey(key));
        }
    }
	if (m_variantCombo && m_model) {
		int curr = m_variantCombo->currentIndex();
		m_variantCombo->blockSignals(true);
		m_variantCombo->clear();
		for (int i = 0; i < m_model->variantCount(); ++i) m_variantCombo->addItem(QString::number(i+1));
		m_variantCombo->setCurrentIndex(qMax(0, curr));
		m_variantCombo->blockSignals(false);
	}
}

