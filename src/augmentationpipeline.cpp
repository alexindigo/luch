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
bool AugmentationPipeline::phaseFromString(const QString &name,
                                           Phase &phase)
{
    if (name == QLatin1String("unwrap")) {
        phase = Phase::Unwrap;
        return true;
    }
    if (name == QLatin1String("clean")) {
        phase = Phase::Clean;
        return true;
    }
    if (name == QLatin1String("detect")) {
        phase = Phase::Detect;
        return true;
    }
    return false; // closed set — anything else is invalid
}

// static
QString AugmentationPipeline::phaseToString(Phase phase)
{
    switch (phase) {
    case Phase::Unwrap:
        return QStringLiteral("unwrap");
    case Phase::Detect:
        return QStringLiteral("detect");
    case Phase::Clean:
        break;
    }
    return QStringLiteral("clean");
}

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
    Phase phase;
    if (id.isEmpty() || id != base || !QFile::exists(soPath)
        || !phaseFromString(
            obj.value(QStringLiteral("phase")).toString(), phase))
        return false;
    manifest.id = id;
    manifest.title = obj.value(QStringLiteral("title")).toString();
    manifest.description =
        obj.value(QStringLiteral("description")).toString();
    manifest.phase = phase;
    manifest.inet =
        obj.value(QStringLiteral("inet")).toBool(); // default false
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

    // Phase-ladder order: Unwrap → Clean → Detect. Within Unwrap:
    // offline (inet:false) unwrap to fixpoint, then online fallback.
    // Within Clean/Detect: id order (settings override later).
    std::sort(manifests.begin(), manifests.end(),
              [](const Manifest &a, const Manifest &b) {
                  if (a.phase != b.phase)
                      return a.phase < b.phase;
                  if (a.phase == Phase::Unwrap && a.inet != b.inet)
                      return !a.inet; // offline unwrap before online
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
                          {QStringLiteral("phase"),
                           phaseToString(manifest.phase)},
                          {QStringLiteral("inet"), manifest.inet},
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
        m_plugins.append({manifest.id, manifest.phase, manifest.inet,
                          plugin});
        entry[QStringLiteral("loaded")] = true;
        m_roster.append(entry);
    }
}

QVariantMap AugmentationPipeline::run(const QVariantMap &targetMap) const
{
    QVariantList trace;
    QVariantList detected;
    QString working = targetMap.value(QStringLiteral("url")).toString();
    const QString original = working;

    for (const LoadedPlugin &p : m_plugins) {
        // Fixpoint (Matryoshka): feed the plugin its own output until
        // it noops or the hop cap dies on adversarial ping-pong.
        for (int hop = 1; hop <= m_maxHops; ++hop) {
            QVariantMap chainState = targetMap;
            chainState.insert(QStringLiteral("url"), working);
            chainState.insert(QStringLiteral("original"), original);
            const QVariantMap slice = p.plugin->augment(chainState, {});

            // Reserved-key sanitation: a "url" that is not a nonempty
            // string is an invalid slice — drop the key, keep the rest.
            QVariantMap data = slice;
            if (data.contains(QStringLiteral("url"))) {
                const QVariant urlValue =
                    data.value(QStringLiteral("url"));
                if (!urlValue.isValid()
                    || urlValue.metaType().id() != QMetaType::QString
                    || urlValue.toString().isEmpty()) {
                    qWarning().noquote()
                        << "luch: dropping invalid url from slice:"
                        << p.id;
                    data.remove(QStringLiteral("url"));
                }
            }

            trace.append(QVariantMap{
                {QStringLiteral("plugin"), p.id},
                {QStringLiteral("iteration"), hop},
                {QStringLiteral("data"), data}});

            if (p.phase == Phase::Detect
                && !data.value(QStringLiteral("verdict")).isNull())
                detected.append(QVariantMap{
                    {QStringLiteral("plugin"), p.id},
                    {QStringLiteral("verdict"),
                     data.value(QStringLiteral("verdict"))},
                    {QStringLiteral("source"),
                     data.value(QStringLiteral("source"))}});

            const QString out =
                data.value(QStringLiteral("url")).toString();
            if (out.isEmpty() || out == working)
                break; // noop → fixpoint reached for this stage
            working = out;
        }
    }

    return {{QStringLiteral("original"), targetMap},
            {QStringLiteral("url"), working},
            {QStringLiteral("detected"), detected},
            {QStringLiteral("trace"), trace}};
}

QVariantList AugmentationPipeline::roster() const
{
    return m_roster;
}

} // namespace Luch
