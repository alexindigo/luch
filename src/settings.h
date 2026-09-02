#pragma once

#include <QObject>
#include <QVariantMap>

namespace Luch {

// Unified settings, owned by the minimal settings daemon:
// ~/.config/luch/settings.json
//   {"ui": {"showDissection": false},
//    "maxHops": 10,
//    "logPayload": false,
//    "plugins": {"<id>": {"enabled": true, "inet": false}}}
//
// config.json stays purely user-authored (browsers / rules / focus).
// Pickers are short-lived and read settings at startup — they never
// write. Migration: per-plugin ~/.config/luch/plugins/<id>.json files
// (the superseded url-cleaning convention) are read once if present
// and merged into the plugins map.
class Settings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool showDissection READ showDissection
               WRITE setShowDissection NOTIFY showDissectionChanged)
    Q_PROPERTY(bool logPayload READ logPayload WRITE setLogPayload
               NOTIFY logPayloadChanged)
    Q_PROPERTY(int maxHops READ maxHops WRITE setMaxHops NOTIFY
               maxHopsChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    static QString settingsPath();

    bool showDissection() const { return m_ui.showDissection; }
    void setShowDissection(bool on);

    bool logPayload() const { return m_logPayload; }
    void setLogPayload(bool on);

    int maxHops() const { return m_maxHops; }
    void setMaxHops(int hops);

    // {"<id>": {"enabled": …, "inet": …, …plugin-specific keys}}
    QVariantMap pluginConfigs() const { return m_plugins; }

    // Materializes settings.json (also persists the one-time migration
    // of legacy per-plugin files). The daemon calls this at startup;
    // toggles save implicitly.
    Q_INVOKABLE void save();

Q_SIGNALS:
    void showDissectionChanged();
    void logPayloadChanged();
    void maxHopsChanged();

private:
    void load();
    void migrateLegacyPluginConfigs();

    struct Ui {
        bool showDissection = false;
    } m_ui;
    int m_maxHops = 10;
    bool m_logPayload = false;
    QVariantMap m_plugins;
};

} // namespace Luch
