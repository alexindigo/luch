#include "augmentationpipeline.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPluginLoader>
#include <QSet>
#include <QStandardPaths>
#include <QThread>

#include "worker/inetworker.h"
#include "worker/offlineworker.h"
#include "worker/workerrunner.h"

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

AugmentationPipeline::~AugmentationPipeline()
{
    // Workers are QObject children — stop their loops before the base
    // destructor tears them down.
    if (m_offlineWorker) {
        m_offlineWorker->quit();
        m_offlineWorker->wait();
    }
    if (m_inetWorker) {
        m_inetWorker->quit();
        m_inetWorker->wait();
    }
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

    // Two shared workers: offline (seccomp network sandbox) and inet
    // (event loop, unfiltered). Runners live on their threads.
    m_offlineWorker = new OfflineWorker(this);
    m_offlineRunner = new WorkerRunner();
    m_offlineRunner->moveToThread(m_offlineWorker);
    connect(m_offlineWorker, &QThread::finished, m_offlineRunner,
            &QObject::deleteLater);
    m_inetWorker = new InetWorker(this);
    m_inetRunner = new WorkerRunner();
    m_inetRunner->moveToThread(m_inetWorker);
    connect(m_inetWorker, &QThread::finished, m_inetRunner,
            &QObject::deleteLater);
    m_offlineWorker->start();
    m_inetWorker->start();

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
        // Privacy-first: declared-online plugins are disabled by
        // default — nothing phones home until the user opts in.
        // Offline plugins default to enabled.
        const bool enabled =
            userConfig.value(QStringLiteral("enabled"),
                             !manifest.inet).toBool();
        // Downgrade-only veto: user config may force a declared-online
        // plugin onto the blocked worker; nothing escalates online.
        const bool effectiveInet =
            manifest.inet
            && userConfig.value(QStringLiteral("inet"), true).toBool();
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
        // Object affinity: the plugin lives on its worker thread.
        if (QObject *obj = qobject_cast<QObject *>(instance)) {
            QThread *worker =
                effectiveInet ? static_cast<QThread *>(m_inetWorker)
                              : static_cast<QThread *>(m_offlineWorker);
            obj->moveToThread(worker);
        }
        m_plugins.append({manifest.id, manifest.phase, manifest.inet,
                          effectiveInet, plugin});
        entry[QStringLiteral("loaded")] = true;
        m_roster.append(entry);
    }

    if (auto *runner = qobject_cast<WorkerRunner *>(m_inetRunner.data()))
        connect(runner, &WorkerRunner::inetDone, this,
                &AugmentationPipeline::onInetDone);
}

QVariantMap AugmentationPipeline::run(const QVariantMap &targetMap)
{
    auto state = QSharedPointer<ChainState>::create();
    state->targetMap = targetMap;
    state->working = targetMap.value(QStringLiteral("url")).toString();
    state->original = state->working;
    runChain(state);
    return payloadOf(state);
}

QVariantMap AugmentationPipeline::chainStateMap(
    const QSharedPointer<ChainState> &state) const
{
    QVariantMap chainState = state->targetMap;
    chainState.insert(QStringLiteral("url"), state->working);
    chainState.insert(QStringLiteral("original"), state->original);
    return chainState;
}

QVariantMap AugmentationPipeline::priorSlicesFrom(
    const QVariantList &trace)
{
    // priorSlices is the read-only side channel: last recorded slice
    // per plugin (non-URL data from specific predecessors).
    QVariantMap slices;
    for (const QVariant &entry : trace) {
        const QVariantMap e = entry.toMap();
        const QString id =
            e.value(QStringLiteral("plugin")).toString();
        if (!id.isEmpty())
            slices.insert(id,
                          e.value(QStringLiteral("data")).toMap());
    }
    return slices;
}

QVariantMap AugmentationPipeline::recordSlice(
    const QSharedPointer<ChainState> &state, const LoadedPlugin &p,
    const QVariantMap &slice)
{
    // Reserved-key sanitation: a "url" that is not a nonempty string
    // is an invalid slice — drop the key, keep the rest.
    QVariantMap data = slice;
    if (data.contains(QStringLiteral("url"))) {
        const QVariant urlValue = data.value(QStringLiteral("url"));
        if (!urlValue.isValid()
            || urlValue.metaType().id() != QMetaType::QString
            || urlValue.toString().isEmpty()) {
            qWarning().noquote()
                << "luch: dropping invalid url from slice:" << p.id;
            data.remove(QStringLiteral("url"));
        }
    }

    state->trace.append(QVariantMap{
        {QStringLiteral("plugin"), p.id},
        {QStringLiteral("iteration"), state->hop},
        {QStringLiteral("data"), data}});

    if (p.phase == Phase::Detect
        && !data.value(QStringLiteral("verdict")).isNull())
        state->detected.append(QVariantMap{
            {QStringLiteral("plugin"), p.id},
            {QStringLiteral("verdict"),
             data.value(QStringLiteral("verdict"))},
            {QStringLiteral("source"),
             data.value(QStringLiteral("source"))}});

    // After the chain paused on an inet stage, run()'s payload is
    // already out — every slice lands via the signal instead.
    if (state->async)
        Q_EMIT sliceLanded(p.id, data);

    return data;
}

void AugmentationPipeline::advanceChain(
    const QSharedPointer<ChainState> &state, const QString &outUrl)
{
    if (!outUrl.isEmpty() && outUrl != state->working) {
        state->working = outUrl; // next fixpoint hop of the same stage
        state->hop++;
    } else {
        state->stage++; // stage reached its fixpoint — next stage
        state->hop = 1;
    }
}

void AugmentationPipeline::runChain(
    const QSharedPointer<ChainState> &state)
{
    while (state->stage < m_plugins.size()) {
        const LoadedPlugin &p = m_plugins.at(state->stage);
        if (state->hop > m_maxHops) {
            // Cap reached — adversarial ping-pong dies here.
            state->stage++;
            state->hop = 1;
            continue;
        }
        if (p.effectiveInet) {
            dispatchInetHop(state, p);
            return; // chain continues when the slice lands
        }
        runOfflineHop(state, p);
    }
}

void AugmentationPipeline::runOfflineHop(
    const QSharedPointer<ChainState> &state, const LoadedPlugin &p)
{
    const QVariantMap chainState = chainStateMap(state);
    const QVariantMap priorSlices = priorSlicesFrom(state->trace);
    QVariantMap slice;

    if (QObject *runner = m_offlineRunner.data()) {
        // Sequential, blocking dispatch: offline stages are µs-fast —
        // serialization is free — and the caller waits for the fixpoint.
        QMetaObject::invokeMethod(
            runner,
            [&slice, p, chainState, priorSlices] {
                slice = p.plugin->augment(chainState, priorSlices);
            },
            Qt::BlockingQueuedConnection);
    } else {
        qWarning("luch: offline worker unavailable — running stage on "
                 "the calling thread");
        slice = p.plugin->augment(chainState, priorSlices);
    }

    const QVariantMap data = recordSlice(state, p, slice);
    advanceChain(state, data.value(QStringLiteral("url")).toString());
}

void AugmentationPipeline::dispatchInetHop(
    const QSharedPointer<ChainState> &state, const LoadedPlugin &p)
{
    QObject *runner = m_inetRunner.data();
    if (!runner) {
        qWarning("luch: inet worker unavailable — dropping stage %s",
                 qPrintable(p.id));
        state->stage++;
        state->hop = 1;
        return;
    }

    const quint64 token = m_nextToken++;
    state->token = token;
    state->async = true; // payload snapshot is out; slices land by signal
    m_activeChains.insert(token, state);
    Q_EMIT stageDispatched(p.id);

    const QVariantMap chainState = chainStateMap(state);
    const QVariantMap priorSlices = priorSlicesFrom(state->trace);

    // Queued, non-blocking dispatch: the plugin runs on the inet
    // worker's event loop; its slice comes back via inetDone and the
    // chain continues there.
    QMetaObject::invokeMethod(
        runner,
        [this, runner, token, p, chainState, priorSlices] {
            const QVariantMap slice =
                p.plugin->augment(chainState, priorSlices);
            emit static_cast<WorkerRunner *>(runner)
                ->inetDone(token, slice);
        },
        Qt::QueuedConnection);
}

void AugmentationPipeline::onInetDone(quint64 token,
                                      const QVariantMap &slice)
{
    const auto it = m_activeChains.constFind(token);
    if (it == m_activeChains.constEnd())
        return; // late result for a finished/gone chain — dropped
    const QSharedPointer<ChainState> state = it.value();
    m_activeChains.erase(it);
    state->token = 0;

    const LoadedPlugin &p = m_plugins.at(state->stage);
    const QVariantMap data = recordSlice(state, p, slice);
    advanceChain(state, data.value(QStringLiteral("url")).toString());
    runChain(state);
}

QVariantMap AugmentationPipeline::payloadOf(
    const QSharedPointer<ChainState> &state) const
{
    return {{QStringLiteral("original"), state->targetMap},
            {QStringLiteral("url"), state->working},
            {QStringLiteral("detected"), state->detected},
            {QStringLiteral("trace"), state->trace}};
}

QVariantList AugmentationPipeline::roster() const
{
    return m_roster;
}

} // namespace Luch
