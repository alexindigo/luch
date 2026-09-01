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
class AugmentationPipeline : public QObject
{
    Q_OBJECT

public:
    explicit AugmentationPipeline(QObject *parent = nullptr);

    void discoverAndLoad(); // once, at startup
    // {"original": targetMap, "plugins": {id: slice, …}}
    QVariantMap run(const QVariantMap &targetMap) const;
    // [{id, title, description, enabled, loaded}]
    QVariantList roster() const;

Q_SIGNALS:
    void payloadChanged(); // reserved for future async slice patching

private:
    struct Manifest {
        QString id;
        QString title;
        QString description;
        int priority = 0;
        QString pluginPath; // <id>.so
    };

    QStringList pluginDirs() const;
    QVariantMap userConfigFor(const QString &id) const;
    // Reads + validates a sidecar manifest ({id,title,description,
    // priority} where id must equal the file's base name).
    static bool readManifest(const QString &manifestPath,
                             Manifest &manifest);

    struct LoadedPlugin {
        QString id;
        LuchTargetAugmenter *plugin = nullptr;
    };
    QList<LoadedPlugin> m_plugins;
    QVariantList m_roster;
    bool m_discovered = false;
};

} // namespace Luch
