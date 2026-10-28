#include "projectmodel.h"

#include <QJsonArray>
#include <QStringList>

ProjectModel::ProjectModel(QObject *parent)
	: QObject(parent)
	, m_rows(0)
	, m_cols(0)
    , m_variantCount(1)
    , m_currentVariant(0)
    , m_checks(1)
	, m_hasStart(false)
	, m_start(-1, -1)
	, m_hasParking(false)
	, m_parking(-1, -1)
{
}

void ProjectModel::resizeGrid(int rows, int cols)
{
	if (rows <= 0) rows = 1;
	if (cols <= 0) cols = 1;
	ensureVariantStorage();
	VariantState s = makeCurrentState();
	// New grid for current variant only
	QVector<QVector<CellType>> newCells(rows, QVector<CellType>(cols, Empty));
	QVector<QVector<bool>> newWallR(rows, QVector<bool>(cols, false));
	QVector<QVector<bool>> newWallB(rows, QVector<bool>(cols, false));
	for (int i = 0; i < qMin(rows, m_rows); ++i) {
		for (int j = 0; j < qMin(cols, m_cols); ++j) {
			newCells[i][j] = m_cells[i][j];
			newWallR[i][j] = m_wallRight[i][j];
			newWallB[i][j] = m_wallBottom[i][j];
		}
	}
	s.rows = rows;
	s.cols = cols;
	s.cells = newCells;
	s.wallRight = newWallR;
	s.wallBottom = newWallB;
	if (!(m_hasStart && m_start.x() >= 0 && m_start.x() < rows && m_start.y() >= 0 && m_start.y() < cols)) {
		s.hasStart = false;
		s.start = QPoint(-1, -1);
	}
	if (!(m_hasParking && m_parking.x() >= 0 && m_parking.x() < rows && m_parking.y() >= 0 && m_parking.y() < cols)) {
		s.hasParking = false;
		s.parking = QPoint(-1, -1);
	}
	m_variants[m_currentVariant] = s;
	applyStateToCurrent(s);
	emit changed();
}

void ProjectModel::resizeGridSilent(int rows, int cols)
{
	// Версия resizeGrid без emit changed() - для использования при загрузке
	if (rows <= 0) rows = 1;
	if (cols <= 0) cols = 1;
	m_rows = rows;
	m_cols = cols;
	m_cells = QVector<QVector<CellType>>(rows, QVector<CellType>(cols, Empty));
	m_wallRight = QVector<QVector<bool>>(rows, QVector<bool>(cols, false));
	m_wallBottom = QVector<QVector<bool>>(rows, QVector<bool>(cols, false));
	// Не очищаем start/parking при загрузке - они будут установлены из загруженных данных
}

void ProjectModel::ensureVariantStorage()
{
	if (m_variants.isEmpty()) {
		m_variants.resize(m_variantCount);
		for (int i = 0; i < m_variantCount; ++i) {
			m_variants[i] = makeCurrentState();
		}
	}
}

ProjectModel::VariantState ProjectModel::makeCurrentState() const
{
	VariantState s;
	s.rows = m_rows;
	s.cols = m_cols;
	s.cells = m_cells;
	s.wallRight = m_wallRight;
	s.wallBottom = m_wallBottom;
	s.hasStart = m_hasStart;
	s.start = m_start;
	s.hasParking = m_hasParking;
	s.parking = m_parking;
	return s;
}

void ProjectModel::applyStateToCurrent(const VariantState &s)
{
	int r = s.rows > 0 ? s.rows : (s.cells.isEmpty() ? 1 : s.cells.size());
	int c = s.cols > 0 ? s.cols : (s.cells.isEmpty() || s.cells[0].isEmpty() ? 1 : s.cells[0].size());
	m_rows = r;
	m_cols = c;
	m_cells = s.cells;
	m_wallRight = s.wallRight;
	m_wallBottom = s.wallBottom;
	m_hasStart = s.hasStart;
	m_start = s.start;
	m_hasParking = s.hasParking;
	m_parking = s.parking;
}

void ProjectModel::setVariantCount(int count)
{
	if (count < 0) count = 0; // Разрешаем 0 для очистки
	if (count == 0) {
		// Очищаем все варианты
		m_variants.clear();
		m_variantCount = 0;
		m_currentVariant = 0;
		return;
	}
	if (count < 1) count = 1;
	ensureVariantStorage();
	// save current
	m_variants[m_currentVariant] = makeCurrentState();
	int old = m_variantCount;
	m_variantCount = count;
	m_variants.resize(m_variantCount);
	for (int i = old; i < m_variantCount; ++i) {
		m_variants[i] = m_variants[qMax(0, m_currentVariant)];
	}
	if (m_currentVariant >= m_variantCount) {
		m_currentVariant = m_variantCount - 1;
	}
    if (m_checks > m_variantCount) m_checks = m_variantCount;
	applyStateToCurrent(m_variants[m_currentVariant]);
	emit changed();
}

void ProjectModel::setCurrentVariantIndex(int index)
{
	if (index < 0 || index >= m_variantCount) return;
	ensureVariantStorage();
	m_variants[m_currentVariant] = makeCurrentState();
	m_currentVariant = index;
	applyStateToCurrent(m_variants[m_currentVariant]);
	emit variantSwitched(m_currentVariant);
	emit changed();
}

void ProjectModel::setChecks(int c)
{
    if (c < 1) c = 1;
    if (c > m_variantCount) c = m_variantCount;
    m_checks = c;
    emit changed();
}

ProjectModel::CellType ProjectModel::cellType(int i, int j) const
{
	return m_cells.at(i).at(j);
}

void ProjectModel::setCellType(int i, int j, CellType type)
{
	ensureValidCell(i, j);
	m_cells[i][j] = type;
	emit changed();
}

void ProjectModel::setStartCell(int i, int j)
{
	ensureValidCell(i, j);
	m_hasStart = true;
	m_start = QPoint(i, j);
	emit changed();
}

void ProjectModel::clearStart()
{
    m_hasStart = false;
    m_start = QPoint(-1, -1);
    emit changed();
}

void ProjectModel::setParkingCell(int i, int j)
{
	ensureValidCell(i, j);
	m_hasParking = true;
	m_parking = QPoint(i, j);
	emit changed();
}

void ProjectModel::clearParking()
{
    m_hasParking = false;
    m_parking = QPoint(-1, -1);
    emit changed();
}

bool ProjectModel::hasStart() const { return m_hasStart; }
QPoint ProjectModel::startCell() const { return m_start; }
bool ProjectModel::hasParking() const { return m_hasParking; }
QPoint ProjectModel::parkingCell() const { return m_parking; }

bool ProjectModel::wall(int i, int j, WallSide side) const
{
	switch (side) {
	case Left:
		return (j > 0) ? m_wallRight[i][j-1] : false; // interior only
	case Right:
		return (j < m_cols - 1) ? m_wallRight[i][j] : false;
	case Top:
		return (i > 0) ? m_wallBottom[i-1][j] : false;
	case Bottom:
		return (i < m_rows - 1) ? m_wallBottom[i][j] : false;
	}
	return false;
}

void ProjectModel::setWall(int i, int j, WallSide side, bool present)
{
	ensureValidCell(i, j);
	if (!isInteriorEdge(i, j, side)) return;
	switch (side) {
	case Left:
		m_wallRight[i][j-1] = present;
		break;
	case Right:
		m_wallRight[i][j] = present;
		break;
	case Top:
		m_wallBottom[i-1][j] = present;
		break;
	case Bottom:
		m_wallBottom[i][j] = present;
		break;
	}
	emit changed();
}

void ProjectModel::toggleWall(int i, int j, WallSide side)
{
	bool cur = wall(i, j, side);
	setWall(i, j, side, !cur);
}

void ProjectModel::toJson(QJsonObject &out) const
{
	// Сохраняем текущее состояние перед сохранением
	// Это гарантирует, что текущий вариант будет сохранен правильно
	ProjectModel *nonConstThis = const_cast<ProjectModel*>(this);
	nonConstThis->ensureVariantStorage();
	nonConstThis->m_variants[m_currentVariant] = makeCurrentState();
	
	out["rows"] = m_rows;
	out["cols"] = m_cols;
    out["variants"] = m_variantCount;
    out["checks"] = m_checks;
	QJsonArray variants;
	for (int vi = 0; vi < m_variantCount; ++vi) {
		VariantState s = m_variants.value(vi, makeCurrentState());
		int vr = s.rows > 0 ? s.rows : (s.cells.isEmpty() ? m_rows : s.cells.size());
		int vc = s.cols > 0 ? s.cols : (s.cells.isEmpty() || s.cells[0].isEmpty() ? m_cols : s.cells[0].size());
		QJsonObject v;
		v["rows"] = vr;
		v["cols"] = vc;
		QJsonArray cells;
		for (int i = 0; i < vr; ++i) {
			QJsonArray row;
			for (int j = 0; j < vc; ++j) row.append(static_cast<int>(s.cells[i][j]));
			cells.append(row);
		}
		v["cells"] = cells;
		QJsonArray wallR;
		for (int i = 0; i < vr; ++i) {
			QJsonArray row;
			for (int j = 0; j < vc; ++j) row.append(s.wallRight[i][j]);
			wallR.append(row);
		}
		v["wallRight"] = wallR;
		QJsonArray wallB;
		for (int i = 0; i < vr; ++i) {
			QJsonArray row;
			for (int j = 0; j < vc; ++j) row.append(s.wallBottom[i][j]);
			wallB.append(row);
		}
		v["wallBottom"] = wallB;
		if (s.hasStart) { QJsonArray st; st.append(s.start.x()); st.append(s.start.y()); v["start"] = st; }
		if (s.hasParking) { QJsonArray pk; pk.append(s.parking.x()); pk.append(s.parking.y()); v["parking"] = pk; }
		variants.append(v);
	}
	out["data"] = variants;
}

bool ProjectModel::fromJson(const QJsonObject &obj)
{
	if (!obj.contains("rows") || !obj.contains("cols")) return false;
	int r = obj.value("rows").toInt();
	int c = obj.value("cols").toInt();
	// Используем silent версию, чтобы не вызывать changed() до загрузки всех данных
	resizeGridSilent(r, c);
	
	QJsonArray arr = obj.value("data").toArray();
	int vcount = obj.value("variants").toInt(1);
	
	// Если есть массив data, используем его размер как количество вариантов
	// Это гарантирует правильную загрузку всех сохраненных вариантов
	if (!arr.isEmpty() && arr.size() > 0) {
		// Используем максимум из поля variants и размера массива data
		vcount = qMax(vcount, arr.size());
	}
	
	m_variantCount = qMax(1, vcount);
	m_currentVariant = 0;
	m_variants.clear();
	m_variants.resize(m_variantCount);
	
	if (arr.isEmpty()) {
		// backward compatibility: single-state format
		VariantState s;
		s.rows = m_rows;
		s.cols = m_cols;
		QJsonArray cells = obj.value("cells").toArray();
		s.cells = QVector<QVector<CellType>>(m_rows, QVector<CellType>(m_cols, Empty));
		for (int i = 0; i < m_rows && i < cells.size(); ++i) {
			QJsonArray row = cells.at(i).toArray();
			for (int j = 0; j < m_cols && j < row.size(); ++j) {
				s.cells[i][j] = static_cast<CellType>(row.at(j).toInt());
			}
		}
		QJsonArray wallR = obj.value("wallRight").toArray();
		s.wallRight = QVector<QVector<bool>>(m_rows, QVector<bool>(m_cols, false));
		for (int i = 0; i < m_rows && i < wallR.size(); ++i) {
			QJsonArray row = wallR.at(i).toArray();
			for (int j = 0; j < m_cols && j < row.size(); ++j) {
				s.wallRight[i][j] = row.at(j).toBool();
			}
		}
		QJsonArray wallB = obj.value("wallBottom").toArray();
		s.wallBottom = QVector<QVector<bool>>(m_rows, QVector<bool>(m_cols, false));
		for (int i = 0; i < m_rows && i < wallB.size(); ++i) {
			QJsonArray row = wallB.at(i).toArray();
			for (int j = 0; j < m_cols && j < row.size(); ++j) {
				s.wallBottom[i][j] = row.at(j).toBool();
			}
		}
		if (obj.contains("start")) { QJsonArray st = obj.value("start").toArray(); if (st.size()==2){ s.hasStart=true; s.start=QPoint(st.at(0).toInt(), st.at(1).toInt()); } }
		if (obj.contains("parking")) { QJsonArray pk = obj.value("parking").toArray(); if (pk.size()==2){ s.hasParking=true; s.parking=QPoint(pk.at(0).toInt(), pk.at(1).toInt()); } }
		for (int i = 0; i < m_variantCount; ++i) m_variants[i] = s;
	} else {
		int loadedCount = qMin(arr.size(), m_variantCount);
		for (int vi = 0; vi < loadedCount; ++vi) {
			QJsonObject v = arr.at(vi).toObject();
			VariantState s;
			int vr = v.value("rows").toInt(0);
			int vc = v.value("cols").toInt(0);
			if (vr <= 0) vr = m_rows;
			if (vc <= 0) vc = m_cols;
			s.rows = vr;
			s.cols = vc;
			QJsonArray cells = v.value("cells").toArray();
			s.cells = QVector<QVector<CellType>>(vr, QVector<CellType>(vc, Empty));
			for (int i = 0; i < vr && i < cells.size(); ++i) {
				QJsonArray row = cells.at(i).toArray();
				for (int j = 0; j < vc && j < row.size(); ++j) s.cells[i][j] = static_cast<CellType>(row.at(j).toInt());
			}
			QJsonArray wallR = v.value("wallRight").toArray();
			s.wallRight = QVector<QVector<bool>>(vr, QVector<bool>(vc, false));
			for (int i = 0; i < vr && i < wallR.size(); ++i) {
				QJsonArray row = wallR.at(i).toArray();
				for (int j = 0; j < vc && j < row.size(); ++j) s.wallRight[i][j] = row.at(j).toBool();
			}
			QJsonArray wallB = v.value("wallBottom").toArray();
			s.wallBottom = QVector<QVector<bool>>(vr, QVector<bool>(vc, false));
			for (int i = 0; i < vr && i < wallB.size(); ++i) {
				QJsonArray row = wallB.at(i).toArray();
				for (int j = 0; j < vc && j < row.size(); ++j) s.wallBottom[i][j] = row.at(j).toBool();
			}
			if (v.contains("start")) {
				QJsonArray st = v.value("start").toArray();
				if (st.size() == 2) {
					s.hasStart = true;
					s.start = QPoint(st.at(0).toInt(), st.at(1).toInt());
				} else {
					s.hasStart = false;
					s.start = QPoint(-1, -1);
				}
			} else {
				s.hasStart = false;
				s.start = QPoint(-1, -1);
			}
			if (v.contains("parking")) {
				QJsonArray pk = v.value("parking").toArray();
				if (pk.size() == 2) {
					s.hasParking = true;
					s.parking = QPoint(pk.at(0).toInt(), pk.at(1).toInt());
				} else {
					s.hasParking = false;
					s.parking = QPoint(-1, -1);
				}
			} else {
				s.hasParking = false;
				s.parking = QPoint(-1, -1);
			}
			m_variants[vi] = s;
		}
		for (int vi = loadedCount; vi < m_variantCount; ++vi) {
			m_variants[vi] = (loadedCount > 0) ? m_variants[0] : makeCurrentState();
		}
	}
    // checks
    int chk = obj.value("checks").toInt(1);
    if (chk < 1) chk = 1; if (chk > m_variantCount) chk = m_variantCount;
    m_checks = chk;

    // Убеждаемся, что у нас есть хотя бы один вариант
    if (m_variants.isEmpty() || m_variants.size() <= m_currentVariant) {
        // Если вариантов нет, создаем пустой
        ensureVariantStorage();
    }
    
    // Применяем состояние первого варианта к текущему
    if (m_variants.size() > m_currentVariant) {
        applyStateToCurrent(m_variants[m_currentVariant]);
    } else if (!m_variants.isEmpty()) {
        applyStateToCurrent(m_variants[0]);
        m_currentVariant = 0;
    }
    
	emit changed();
	return true;
}

QString ProjectModel::generatePythonTask(const QString &taskId) const
{
	QStringList lines;
	lines << "#!/usr/bin/python3";
	lines << "";
	lines << "import pyrob.core as rob";
    if (m_variantCount > 1 || m_checks > 1) lines << "import random";
	// Determine if any variant uses ToBeFilled
	bool needsHelpers = false;
	for (int vi = 0; vi < m_variantCount; ++vi) {
		const VariantState s = (vi == m_currentVariant) ? makeCurrentState() : m_variants.value(vi, makeCurrentState());
		int vr = s.rows > 0 ? s.rows : (s.cells.isEmpty() ? 1 : s.cells.size());
		int vc = s.cols > 0 ? s.cols : (s.cells.isEmpty() || s.cells[0].isEmpty() ? 1 : s.cells[0].size());
		for (int i = 0; i < vr && !needsHelpers; ++i) {
			for (int j = 0; j < vc; ++j) { if (s.cells[i][j] == ToBeFilled) { needsHelpers = true; break; } }
			if (needsHelpers) break;
		}
		if (needsHelpers) break;
	}
	if (needsHelpers) lines << "from pyrob.tasks import check_filled_cells, find_cells_to_be_filled";
	lines << "";
	if (m_variantCount > 1) {
		lines << QString("# Порядок вариантов: один раз перемешиваем при загрузке модуля — без повторений в пределах 0..%1").arg(m_variantCount - 1);
		lines << QString("_variant_order = list(range(%1))").arg(m_variantCount);
		lines << "random.shuffle(_variant_order)";
		lines << "";
	}
	lines << "class Task:";
    lines << QString("    CHECKS = %1").arg(m_checks);
	lines << "";
	lines << "    def load_level(self, n):";
    if (m_variantCount > 1) {
        lines << "        idx = _variant_order[n % len(_variant_order)]";
    } else {
        lines << "        idx = 0";
    }
	for (int vi = 0; vi < m_variantCount; ++vi) {
		const VariantState s = (vi == m_currentVariant) ? makeCurrentState() : m_variants.value(vi, makeCurrentState());
		int vr = s.rows > 0 ? s.rows : (s.cells.isEmpty() ? 1 : s.cells.size());
		int vc = s.cols > 0 ? s.cols : (s.cells.isEmpty() || s.cells[0].isEmpty() ? 1 : s.cells[0].size());
		lines << QString("        %1 idx == %2:").arg(vi==0?"if":"elif").arg(vi);
		lines << QString("            rob.set_field_size(%1, %2)").arg(vr).arg(vc);
		for (int i = 0; i < vr; ++i) {
			for (int j = 0; j < vc; ++j) {
				if (s.cells[i][j] == ToBeFilled) lines << QString("            rob.set_cell_type(%1, %2, rob.CELL_TO_BE_FILLED)").arg(i).arg(j);
				else if (s.cells[i][j] == FilledInitial) lines << QString("            rob.set_cell_type(%1, %2, rob.CELL_FILLED)").arg(i).arg(j);
			}
		}
		for (int i = 0; i < vr; ++i) {
			for (int j = 0; j < vc; ++j) {
				QStringList parts; bool any=false;
				if (j < vc-1 && s.wallRight[i][j]) { parts << "right=True"; any=true; }
				if (i < vr-1 && s.wallBottom[i][j]) { parts << "bottom=True"; any=true; }
				if (any) {
					lines << QString("            rob.goto(%1, %2)").arg(i).arg(j);
					lines << QString("            rob.put_wall(%1)").arg(parts.join(", "));
				}
			}
		}
		if (s.hasParking) lines << QString("            rob.set_parking_cell(%1, %2)").arg(s.parking.x()).arg(s.parking.y());
		if (s.hasStart) lines << QString("            rob.goto(%1, %2)").arg(s.start.x()).arg(s.start.y());
	}
	if (needsHelpers) { lines << ""; lines << "        self.cells_to_fill = find_cells_to_be_filled()"; }

	lines << "";
	lines << "    def check_solution(self):";
	QString ret;
	// Evaluate parking condition if any variant has parking
	bool anyParking = false;
	for (int vi = 0; vi < m_variantCount; ++vi) {
		const VariantState s = (vi == m_currentVariant) ? makeCurrentState() : m_variants.value(vi, makeCurrentState());
		if (s.hasParking) { anyParking = true; break; }
	}
	if (needsHelpers && anyParking) ret = "check_filled_cells(self.cells_to_fill) and rob.is_parking_point()";
	else if (needsHelpers) ret = "check_filled_cells(self.cells_to_fill)";
	else if (anyParking) ret = "rob.is_parking_point()";
	else ret = "True";
	lines << QString("        return %1").arg(ret);

	return lines.join("\n") + "\n";
}

void ProjectModel::ensureValidCell(int i, int j) const
{
	if (i < 0 || i >= m_rows || j < 0 || j >= m_cols) {
		qWarning("Invalid cell indices");
	}
}

bool ProjectModel::isInteriorEdge(int i, int j, WallSide side) const
{
	switch (side) {
	case Left:   return j > 0;
	case Right:  return j < m_cols - 1;
	case Top:    return i > 0;
	case Bottom: return i < m_rows - 1;
	}
	return false;
}







