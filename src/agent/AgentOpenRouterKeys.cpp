#include "AgentOpenRouterKeys.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace {

constexpr char kSettingsGroup[] = "agent";
constexpr char kKeysSetting[] = "openrouterKeys";
constexpr char kModelsSetting[] = "openrouterModels";

} // namespace

QVector<OpenRouterKeyEntry> AgentOpenRouterKeys::load()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QByteArray raw = settings.value(QLatin1String(kKeysSetting)).toByteArray();
    settings.endGroup();

    QVector<OpenRouterKeyEntry> entries;
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) {
        return entries;
    }

    const QJsonArray array = doc.array();
    entries.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
        const QString key = obj.value(QStringLiteral("key")).toString().trimmed();
        if (name.isEmpty() || key.isEmpty()) {
            continue;
        }
        entries.append({name, key});
    }
    return entries;
}

void AgentOpenRouterKeys::save(const QVector<OpenRouterKeyEntry> &keys)
{
    QJsonArray array;
    for (const OpenRouterKeyEntry &entry : keys) {
        const QString name = entry.name.trimmed();
        const QString key = entry.key.trimmed();
        if (name.isEmpty() || key.isEmpty()) {
            continue;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), name);
        obj.insert(QStringLiteral("key"), key);
        array.append(obj);
    }

    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QLatin1String(kKeysSetting),
                      QJsonDocument(array).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

QString AgentOpenRouterKeys::keyForName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QVector<OpenRouterKeyEntry> entries = load();
    for (const OpenRouterKeyEntry &entry : entries) {
        if (entry.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return entry.key;
        }
    }
    return QString();
}

bool AgentOpenRouterKeys::nameExists(const QString &name, const QString &ignoreName)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QVector<OpenRouterKeyEntry> entries = load();
    for (const OpenRouterKeyEntry &entry : entries) {
        if (!ignoreName.isEmpty()
            && entry.name.compare(ignoreName.trimmed(), Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (entry.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QStringList AgentOpenRouterKeys::loadModels()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QByteArray raw = settings.value(QLatin1String(kModelsSetting)).toByteArray();
    settings.endGroup();

    QStringList models;
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) {
        return models;
    }

    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        const QString id = value.isString()
            ? value.toString().trimmed()
            : value.toObject().value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty() && !models.contains(id, Qt::CaseInsensitive)) {
            models.append(id);
        }
    }
    return models;
}

void AgentOpenRouterKeys::saveModels(const QStringList &modelIds)
{
    QJsonArray array;
    for (const QString &id : modelIds) {
        const QString trimmed = id.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        array.append(trimmed);
    }

    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QLatin1String(kModelsSetting),
                      QJsonDocument(array).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

bool AgentOpenRouterKeys::modelExists(const QString &modelId)
{
    const QString trimmed = modelId.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    const QStringList models = loadModels();
    for (const QString &id : models) {
        if (id.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
