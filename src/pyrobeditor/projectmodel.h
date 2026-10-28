#ifndef PROJECTMODEL_H
#define PROJECTMODEL_H

#include <QObject>
#include <QVector>
#include <QPoint>
#include <QJsonObject>
#include <QString>

class ProjectModel : public QObject
{
	Q_OBJECT
public:
	enum CellType { Empty = 0, ToBeFilled = 1, FilledInitial = 2 };
	enum WallSide { Left = 0, Right = 1, Top = 2, Bottom = 3 };

	struct VariantState {
		int rows = 0;
		int cols = 0;
		QVector<QVector<CellType>> cells;
		QVector<QVector<bool>> wallRight;
		QVector<QVector<bool>> wallBottom;
		bool hasStart = false;
		QPoint start = QPoint(-1, -1);
		bool hasParking = false;
		QPoint parking = QPoint(-1, -1);
	};

	explicit ProjectModel(QObject *parent = nullptr);

	int rows() const { return m_rows; }
	int cols() const { return m_cols; }
	void resizeGrid(int rows, int cols);
	void resizeGridSilent(int rows, int cols); // Без emit changed() - для загрузки

	// Variants API
	int variantCount() const { return m_variantCount; }
	void setVariantCount(int count);
	int currentVariantIndex() const { return m_currentVariant; }
	void setCurrentVariantIndex(int index);

	int checks() const { return m_checks; }
	void setChecks(int c);

	// Editing reads/writes current variant
	CellType cellType(int i, int j) const;
	void setCellType(int i, int j, CellType type);

	bool hasStart() const;
	QPoint startCell() const;
	void setStartCell(int i, int j);
    void clearStart();

	bool hasParking() const;
	QPoint parkingCell() const;
	void setParkingCell(int i, int j);
    void clearParking();

	bool wall(int i, int j, WallSide side) const;
	void setWall(int i, int j, WallSide side, bool present);
	void toggleWall(int i, int j, WallSide side);

	void toJson(QJsonObject &out) const;
	bool fromJson(const QJsonObject &obj);
	QString generatePythonTask(const QString &taskId) const;

signals:
	void changed();
	void variantSwitched(int index);

private:
	void ensureValidCell(int i, int j) const;
	bool isInteriorEdge(int i, int j, WallSide side) const;
	void ensureVariantStorage();
	VariantState makeCurrentState() const;
	void applyStateToCurrent(const VariantState &s);

private:
	int m_rows;
	int m_cols;
	int m_variantCount;
	int m_currentVariant;
	int m_checks;
	// Current editable state mirrors m_variants[m_currentVariant]
	QVector<QVector<CellType>> m_cells;
	QVector<QVector<bool>> m_wallRight;
	QVector<QVector<bool>> m_wallBottom;
	bool m_hasStart;
	QPoint m_start;
	bool m_hasParking;
	QPoint m_parking;
	QVector<VariantState> m_variants;
};

#endif // PROJECTMODEL_H







