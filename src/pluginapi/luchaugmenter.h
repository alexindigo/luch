#pragma once
#include <QVariantMap>
#include <QtPlugin>

// Luch target-augmenter plugin interface — the ONLY thing crossing the
// ABI besides Qt types. Header-only (interface + IID); no Luch-internal
// structs.
//
// Contract:
//  - Manifest = sidecar <id>.json next to the .so:
//    {"id","title","description","phase","inet"}.
//    "phase" ∈ {"unwrap","clean","detect"} — the closed, system-owned
//    phase ladder; unknown/missing → invalid manifest. "inet"
//    (capability declaration, default false): true → the plugin runs on
//    the network worker and is disabled by default (privacy-first;
//    user opts in). There is no "priority": position is decided by the
//    phase ladder, never by the plugin.
//  - User config = optional ~/.config/luch/plugins/<id>.json
//    ({"enabled": …} + plugin-specific keys); absent file = enabled,
//    empty settings. Passed once via init() after load. Per-plugin
//    config may only DOWNGRADE capabilities (e.g. force a declared
//    online plugin offline); nothing lets a plugin escalate online.
//  - augment() receives the chain state as a map:
//      {"kind": "url"|"htmlfile",
//       "url":  <current working URL (starts as the original)>,
//       "raw":  <raw arg>,
//       "original": <original urlForm>}
//    and priorSlices = {pluginId: slice, …} from earlier plugins
//    (read-only side channel for non-URL data; no plugin ever hardcodes
//    whose "url" to read — the pipeline hands everyone the working URL).
//  - Returns this plugin's slice. Reserved key: "url" — the plugin's
//    output URL, ONLY when it changed the working URL. All other keys
//    plugin-owned. Slice presence carries no meaning: consumers check
//    "url" and nothing else. The reserved set may grow additively;
//    never redefine "url".
//  - Must be fast + synchronous + never throw; invalid slice is dropped.
//  - ABI note: pre-1.0 the vtable may still change (e.g. init() was
//    added after augment()); the IID bumps on any breaking change.
class LuchTargetAugmenter {
public:
    virtual ~LuchTargetAugmenter() = default;
    virtual void init(const QVariantMap &userConfig) { Q_UNUSED(userConfig); }
    virtual QVariantMap augment(const QVariantMap &chainState,
                                const QVariantMap &priorSlices) const = 0;
};
#define LuchTargetAugmenter_iid "app.luch.TargetAugmenter/1.0"
Q_DECLARE_INTERFACE(LuchTargetAugmenter, LuchTargetAugmenter_iid)
