#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QVariantList>
#include <QVariantMap>

#include "pluginapi/luchaugmenter.h"

namespace Luch {

class OfflineWorker;
class InetWorker;
class WorkerRunner;

// Discovers, loads and runs target-augmenter plugins (.so + sidecar
// manifest). Discovery order (first id wins — userland always wins):
//   $LUCH_PLUGINS_DIR → ~/.local/share/luch/plugins → installed dir.
// Per-plugin user config: ~/.config/luch/plugins/<id>.json
// ({"enabled": …} + plugin-specific keys); absent file = enabled.
//
// Execution model — sequential chain over the system-owned phase
// ladder Unwrap → Clean → Detect. Each stage receives chain state
// {kind, url (working), raw, original} and runs to fixpoint: its own
// output is fed back until the slice noops or maxHops is reached.
// Every iteration is recorded as a trace entry
// {plugin, iteration, data}; the pipeline returns the full payload
// {"original": targetMap, "url": <effective>, "detected": […],
//  "trace": […]}. Stage order: phase ladder; within Unwrap
// inet:false before inet:true; within Clean/Detect: id order.
//
// Workers: inet:false stages run on the offline worker (seccomp
// network sandbox, sequential blocking dispatch — µs-fast stages make
// serialization free); inet:true stages run queued on the inet worker
// (event loop) — a slow plugin never blocks the caller, its slice
// lands later via the sliceLanded signal (→ queue.patchSlice) and the
// chain continues from there. A chain that reaches an inet stage
// returns its partial payload; the remainder is delivered patch-wise.
// Late results for finished chains are dropped.
class AugmentationPipeline : public QObject
{
    Q_OBJECT

public:
    explicit AugmentationPipeline(QObject *parent = nullptr);
    ~AugmentationPipeline() override;

    void discoverAndLoad(); // once, at startup; starts the workers
    // Full payload: {"original": targetMap, "url": <effective URL>,
    // "detected": [{plugin, verdict, source}, …],
    // "trace": [{plugin, iteration, data}, …]}
    QVariantMap run(const QVariantMap &targetMap);
    // [{id, title, description, phase, inet, enabled, loaded}]
    QVariantList roster() const;

    // Cap for per-stage fixpoint iteration (Matryoshka). Uniform for
    // every plugin; configurable later via settings.
    int maxHops() const { return m_maxHops; }
    void setMaxHops(int hops) { m_maxHops = qMax(1, hops); }

Q_SIGNALS:
    // A late slice from an inet plugin landed: {plugin → id, data →
    // slice}; the consumer (queue.patchSlice) appends it as the
    // plugin's next trace entry.
    void sliceLanded(const QString &id, const QVariantMap &slice);

private:
    enum class Phase { Unwrap = 0, Clean = 1, Detect = 2 };

    // Per-target chain execution state; shared pointers keep it alive
    // across async inet hops.
    struct ChainState {
        QVariantMap targetMap;
        QString original;
        QString working;
        QVariantList trace;
        QVariantList detected;
        int stage = 0;  // index into m_plugins
        int hop = 1;    // fixpoint iteration within the current stage
        quint64 token = 0; // active inet dispatch (0 = none)
        // true once the chain paused on an inet dispatch — from then
        // on every recorded slice must reach the consumer via
        // sliceLanded (run()'s payload is already a snapshot).
        bool async = false;
    };

    struct Manifest {
        QString id;
        QString title;
        QString description;
        Phase phase = Phase::Clean;
        bool inet = false;
        QString pluginPath; // <id>.so
    };

    struct LoadedPlugin {
        QString id;
        Phase phase = Phase::Clean;
        bool declaredInet = false;
        bool effectiveInet = false; // user veto may downgrade to offline
        LuchTargetAugmenter *plugin = nullptr;
    };

    QStringList pluginDirs() const;
    QVariantMap userConfigFor(const QString &id) const;
    // Reads + validates a sidecar manifest ({id,title,description,
    // phase,inet} where id must equal the file's base name and phase
    // must be in the closed set).
    static bool readManifest(const QString &manifestPath,
                             Manifest &manifest);
    static bool phaseFromString(const QString &name, Phase &phase);
    static QString phaseToString(Phase phase);

    void runChain(const QSharedPointer<ChainState> &state);
    void runOfflineHop(const QSharedPointer<ChainState> &state,
                       const LoadedPlugin &p);
    void dispatchInetHop(const QSharedPointer<ChainState> &state,
                         const LoadedPlugin &p);
    void onInetDone(quint64 token, const QVariantMap &slice);
    QVariantMap chainStateMap(const QSharedPointer<ChainState> &state)
        const;
    static QVariantMap priorSlicesFrom(const QVariantList &trace);
    // Records one iteration: sanitizes the reserved "url" key, appends
    // the trace entry, harvests Detect verdicts. Returns the sanitized
    // slice data.
    QVariantMap recordSlice(const QSharedPointer<ChainState> &state,
                            const LoadedPlugin &p,
                            const QVariantMap &slice);
    void advanceChain(const QSharedPointer<ChainState> &state,
                      const QString &outUrl);
    QVariantMap payloadOf(const QSharedPointer<ChainState> &state) const;

    int m_maxHops = 10;

    OfflineWorker *m_offlineWorker = nullptr;
    InetWorker *m_inetWorker = nullptr;
    QPointer<QObject> m_offlineRunner;
    QPointer<QObject> m_inetRunner;
    QHash<quint64, QSharedPointer<ChainState>> m_activeChains;
    quint64 m_nextToken = 1;

    QList<LoadedPlugin> m_plugins;
    QVariantList m_roster;
    bool m_discovered = false;
};

} // namespace Luch
