#include "settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace Luch {

namespace {

QString configDir()
{
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/luch");
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    load();
}

QString Settings::settingsPath()
{
    return configDir() + QStringLiteral("/settings.json");
}

void Settings::load()
{
    QFile file(settingsPath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            const QJsonObject ui =
                root.value(QStringLiteral("ui")).toObject();
            m_ui.showDissection =
                ui.value(QStringLiteral("showDissection")).toBool();
            m_logPayload =
                root.value(QStringLiteral("logPayload")).toBool();
            m_maxHops = qMax(1, root.value(QStringLiteral("maxHops"))
                                   .toInt(10));
            m_plugins =
                root.value(QStringLiteral("plugins")).toObject()
                    .toVariantMap();
        }
    } else {
        migrateLegacyPluginConfigs();
        return; // no settings.json yet — defaults (+ migration in memory)
    }
    migrateLegacyPluginConfigs();
}

// Legacy convention (superseded): ~/.config/luch/plugins/<id>.json.
// Read once if present; entries missing from the plugins map are
// adopted wholesale ({"enabled": …, "inet": …} + plugin-specific keys).
void Settings::migrateLegacyPluginConfigs()
{
    const QDir dir(configDir() + QStringLiteral("/plugins"));
    if (!dir.exists())
        return;
    const QStringList sidecars =
        dir.entryList({QStringLiteral("*.json")}, QDir::Files,
                      QDir::Name);
    for (const QString &fileName : sidecars) {
        const QString id = fileName.chopped(
            QStringView(QStringLiteral(".json")).size());
        if (m_plugins.contains(id))
            continue;
        QFile file(dir.absoluteFilePath(fileName));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject())
            m_plugins.insert(id, doc.object().toVariantMap());
    }
}

void Settings::setShowDissection(bool on)
{
    if (m_ui.showDissection == on)
        return;
    m_ui.showDissection = on;
    Q_EMIT showDissectionChanged();
    save();
}

void Settings::setLogPayload(bool on)
{
    if (m_logPayload == on)
        return;
    m_logPayload = on;
    Q_EMIT logPayloadChanged();
    save();
}

void Settings::setMaxHops(int hops)
{
    const int capped = qMax(1, hops);
    if (m_maxHops == capped)
        return;
    m_maxHops = capped;
    Q_EMIT maxHopsChanged();
    save();
}

void Settings::save()
{
    const QDir dir(configDir());
    if (!dir.exists())
        QDir().mkpath(configDir());

    QJsonObject root;
    root.insert(QStringLiteral("ui"),
                QJsonObject{{QStringLiteral("showDissection"),
                             m_ui.showDissection}});
    root.insert(QStringLiteral("maxHops"), m_maxHops);
    root.insert(QStringLiteral("logPayload"), m_logPayload);
    root.insert(QStringLiteral("plugins"),
                QJsonObject::fromVariantMap(m_plugins));

    QFile file(settingsPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote()
            << "luch: cannot write settings:" << settingsPath();
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace Luch
