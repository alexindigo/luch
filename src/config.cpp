#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace Luch {

Config::Config(QObject *parent)
    : QObject(parent)
{
    load();
}

const QVector<BrowserEntry> &Config::browsers() const
{
    return m_browsers;
}

QString Config::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/luch/config.json");
}

void Config::load()
{
    const QString path = configPath();
    QFile file(path);
    if (!file.exists()) {
        writeDefaults();
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "luch: cannot read config:" << path;
        return;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote() << "luch: invalid config JSON:" << error.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    for (const QJsonValue &value : root.value(QStringLiteral("browsers")).toArray()) {
        const QJsonObject obj = value.toObject();
        BrowserEntry entry;
        entry.id = obj.value(QStringLiteral("id")).toString();
        entry.name = obj.value(QStringLiteral("name")).toString();
        entry.exec = obj.value(QStringLiteral("exec")).toString();
        entry.icon = obj.value(QStringLiteral("icon")).toString();
        entry.source = obj.value(QStringLiteral("source")).toString();
        entry.hidden = obj.value(QStringLiteral("hidden")).toBool();
        if (entry.id.isEmpty())
            continue;
        m_browsers.append(entry);
    }

    const QJsonArray rules = root.value(QStringLiteral("rules")).toArray();
    if (!rules.isEmpty())
        qInfo().noquote() << QStringLiteral(
            "luch: config contains %1 rule(s) — rules are a v2 feature, ignored")
            .arg(rules.size());
}

void Config::writeDefaults() const
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("ui")] = QJsonObject{
        {QStringLiteral("accent"), QStringLiteral("cyan")},
        {QStringLiteral("theme"), QStringLiteral("light")},
    };
    root[QStringLiteral("browsers")] = QJsonArray();
    root[QStringLiteral("rules")] = QJsonArray();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning().noquote() << "luch: cannot write config:" << path;
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace Luch
