#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct OpenRouterKeyEntry {
    QString name;
    QString key;
};

class AgentOpenRouterKeys {
public:
    static QVector<OpenRouterKeyEntry> load();
    static void save(const QVector<OpenRouterKeyEntry> &keys);
    static QString keyForName(const QString &name);
    static bool nameExists(const QString &name, const QString &ignoreName = QString());

    static QStringList loadModels();
    static void saveModels(const QStringList &modelIds);
    static bool modelExists(const QString &modelId);
};
