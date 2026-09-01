#include "augmentationpipeline.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace Luch {

// static
bool AugmentationPipeline::readManifest(const QString &manifestPath,
                                        Manifest &manifest)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();
    const QString id = obj.value(QStringLiteral("id")).toString();
    const QString base =
        QFileInfo(manifestPath).completeBaseName(); // <id>.json
    const QString soPath =
        QFileInfo(manifestPath).absolutePath() + QLatin1Char('/') + base
        + QStringLiteral(".so");
    if (id.isEmpty() || id != base || !QFile::exists(soPath))
        return false;
    manifest.id = id;
    manifest.title = obj.value(QStringLiteral("title")).toString();
    manifest.description =
        obj.value(QStringLiteral("description")).toString();
    manifest.priority = obj.value(QStringLiteral("priority")).toInt();
    manifest.pluginPath = soPath;
    return true;
}

AugmentationPipeline::AugmentationPipeline(QObject *parent)
    : QObject(parent)
{
}

QStringList AugmentationPipeline::pluginDirs() const
{
    QStringList dirs;
    const QString envDir =
        qEnvironmentVariable("LUCH_PLUGINS_DIR");
    if (!envDir.isEmpty())
        dirs << envDir;
    dirs << QStandardPaths::writableLocation(
                QStandardPaths::GenericDataLocation)
                + QStringLiteral("/luch/plugins");
    dirs << QStringLiteral(LUCH_INSTALLED_PLUGINS_DIR);
    return dirs;
}

QVariantMap AugmentationPipeline::userConfigFor(const QString &id) const
{
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/luch/plugins/") + id + QStringLiteral(".json");
    QFile file(path);
    if (!file.exists())
        return {}; // absent file = enabled, empty settings
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote()
            << "luch: cannot read plugin config:" << path;
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning().noquote()
            << "luch: invalid plugin config JSON:" << path;
        return {};
    }
    return doc.object().toVariantMap();
}

void AugmentationPipeline::discoverAndLoad()
{
    if (m_discovered)
        return;
    m_discovered = true;

    QList<Manifest> manifests;
    QSet<QString> seen; // first id wins — userland before system
    for (const QString &dirPath : pluginDirs()) {
        const QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QStringList sidecars =
            dir.entryList({QStringLiteral("*.json")}, QDir::Files,
                          QDir::Name);
        for (const QString &fileName : sidecars) {
            Manifest manifest;
            if (!readManifest(dir.absoluteFilePath(fileName), manifest)) {
                qWarning().noquote()
                    << "luch: skipping invalid plugin manifest:"
                    << dir.absoluteFilePath(fileName);
                continue;
            }
            if (seen.contains(manifest.id))
                continue; // higher-priority dir already claimed this id
            seen.insert(manifest.id);
            manifests.append(manifest);
        }
    }

    std::sort(manifests.begin(), manifests.end(),
              [](const Manifest &a, const Manifest &b) {
                  if (a.priority != b.priority)
                      return a.priority < b.priority;
                  return a.id < b.id;
              });

    for (const Manifest &manifest : manifests) {
        const QVariantMap userConfig = userConfigFor(manifest.id);
        const bool enabled =
            userConfig.value(QStringLiteral("enabled"), true).toBool();
        QVariantMap entry{{QStringLiteral("id"), manifest.id},
                          {QStringLiteral("title"), manifest.title},
                          {QStringLiteral("description"),
                           manifest.description},
                          {QStringLiteral("enabled"), enabled},
                          {QStringLiteral("loaded"), false}};
        if (!enabled) {
            m_roster.append(entry);
            continue; // never loaded
        }

        QPluginLoader loader(manifest.pluginPath);
        QObject *instance = loader.instance();
        auto *plugin = qobject_cast<LuchTargetAugmenter *>(instance);
        if (!plugin) {
            qWarning().noquote()
                << "luch: plugin failed to load:" << manifest.id
                << (manifest.title.isEmpty() ? QString()
                                             : QStringLiteral(" (%1)")
                                                       .arg(manifest.title))
                << loader.errorString();
            m_roster.append(entry);
            continue;
        }
        plugin->init(userConfig);
        m_plugins.append({manifest.id, plugin});
        entry[QStringLiteral("loaded")] = true;
        m_roster.append(entry);
    }
}

QVariantMap AugmentationPipeline::run(const QVariantMap &targetMap) const
{
    QVariantMap plugins;
    for (const LoadedPlugin &loaded : m_plugins) {
        // priorSlices = the slices accumulated so far (read-only by
        // contract); each plugin derives from the ORIGINAL target only.
        const QVariantMap slice =
            loaded.plugin->augment(targetMap, plugins);
        const QVariant urlValue = slice.value(QStringLiteral("url"));
        if (urlValue.isValid()
            && (urlValue.metaType().id() != QMetaType::QString
                || urlValue.toString().isEmpty())) {
            qWarning().noquote()
                << "luch: dropping invalid slice from plugin:"
                << loaded.id;
            continue;
        }
        plugins.insert(loaded.id, slice);
    }
    return {{QStringLiteral("original"), targetMap},
            {QStringLiteral("plugins"), plugins}};
}

QVariantList AugmentationPipeline::roster() const
{
    return m_roster;
}

} // namespace Luch
