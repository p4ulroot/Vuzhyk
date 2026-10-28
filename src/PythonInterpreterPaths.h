#pragma once

#include <QString>

namespace PythonInterpreterPaths {

QString sitePackagesDir(const QString &pythonExe, bool createIfMissing = false, QString *errorMessage = nullptr);
bool isPyrobInstalled(const QString &pythonExe, QString *tasksDir = nullptr);
QString pyrobTasksDir(const QString &pythonExe, bool *installed = nullptr);
QString embeddedSitePackagesDir(const QString &appDir);

} // namespace PythonInterpreterPaths
