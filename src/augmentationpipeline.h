#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "pluginapi/luchaugmenter.h"

namespace Luch {

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
class AugmentationPipeline : public QObject
{
    Q_OBJECT

public:
    explicit AugmentationPipeline(QObject *parent = nullptr);

    void discoverAndLoad(); // once, at startup
    // Full payload: {"original": targetMap, "url": <effective URL>,
    // "detected": [{plugin, verdict, source}, …],
    // "trace": [{plugin, iteration, data}, …]}
    QVariantMap run(const QVariantMap &targetMap) const;
    // [{id, title, description, phase, inet, enabled, loaded}]
    QVariantList roster() const;

    // Cap for per-stage fixpoint iteration (Matryoshka). Uniform for
    // every plugin; configurable later via settings.
    int maxHops() const { return m_maxHops; }
    void setMaxHops(int hops) { m_maxHops = qMax(1, hops); }

Q_SIGNALS:
    void payloadChanged(); // reserved for async slice patching

private:
    enum class Phase { Unwrap = 0, Clean = 1, Detect = 2 };

    struct Manifest {
        QString id;
        QString title;
        QString description;
        Phase phase = Phase::Clean;
        bool inet = false;
        QString pluginPath; // <id>.so
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

    int m_maxHops = 10;

    struct LoadedPlugin {
        QString id;
        Phase phase = Phase::Clean;
        bool inet = false;
        LuchTargetAugmenter *plugin = nullptr;
    };
    QList<LoadedPlugin> m_plugins;
    QVariantList m_roster;
    bool m_discovered = false;
};

} // namespace Luch
