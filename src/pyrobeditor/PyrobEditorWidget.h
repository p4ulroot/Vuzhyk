#ifndef PYROBEDITORWIDGET_H
#define PYROBEDITORWIDGET_H

#include <QWidget>
#include <QPointer>

class GridEditor;
class ProjectModel;
class QToolBar;
class QToolButton;
class QButtonGroup;
class QSpinBox;
class QComboBox;
class QLabel;
class QMenu;
class QAction;
class MainWindow;

class PyrobEditorWidget : public QWidget
{
	Q_OBJECT
public:
	enum AppLanguage { LangRu = 0, LangEn = 1 };
	explicit PyrobEditorWidget(QWidget *parent = nullptr);
	~PyrobEditorWidget();
	
	void clearAll(); // Явная очистка всех ресурсов
	
	void setTheme(const QString &theme);

private slots:
	void newProject();
	void openProject();
	void saveTask();
	void saveProject();
	void exportPython();
	void gridSizeChanged();
	void setToolIndex(int index);
	void toolSelected(int id);
	void variantsCountChanged(int value);
	void currentVariantChanged(int index);
	void checksChanged(int value);

private:
	void createUi();
	void createActions();
	void createToolbar();
	void retranslateUi();
	QString trKey(const char *key) const;
	QString defaultTasksDirectory(bool forSaveOrExport = false) const;
	bool maybeSave();
	bool saveToFile(const QString &path);
	bool loadFromFile(const QString &path);
	QString showFilePicker(const QString &filter, const QString &initialDir = QString(), bool isSave = false, bool isDirectory = false);

private:
	QPointer<GridEditor> m_editor;
	QPointer<ProjectModel> m_model;
	MainWindow *m_mainWindow { nullptr };
	QString m_currentProjectPath;
	QString m_currentTaskPath;
	QAction *m_newAct;
	QAction *m_openAct;
	QAction *m_saveTaskAct;
	QAction *m_saveAct;
	QAction *m_exportAct;
	QSpinBox *m_rowsSpin;
	QSpinBox *m_colsSpin;
	QSpinBox *m_variantsSpin;
	QSpinBox *m_checksSpin;
	QComboBox *m_variantCombo;
	QButtonGroup *m_toolGroup;
	AppLanguage m_lang;
	QMenu *m_fileMenu;
	QToolButton *m_fileButton;
	QLabel *m_rowsLbl;
	QLabel *m_colsLbl;
	QLabel *m_variantsLbl;
	QLabel *m_variantLbl;
	QLabel *m_checksLbl;
	QLabel *m_toolLbl;
	QToolBar *m_toolBar;
	QString m_theme { "light" };
};

#endif // PYROBEDITORWIDGET_H

