#pragma once

#include <QString>

namespace LibrarySyncServerControl {

QString configFilePath();
bool writeServerConfig(const QString &sitePackagesDir, int port, bool allowAgent, QString *errorMessage = nullptr);
bool ensureAutostartEnabled(QString *errorMessage = nullptr);
bool removeAutostart(QString *errorMessage = nullptr);
bool launchServerIfNeeded(QString *errorMessage = nullptr);
bool stopServerProcess();
QString serverExecutablePath();
void applyFromSettings(QString *errorMessage = nullptr);

} // namespace LibrarySyncServerControl
