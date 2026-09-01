#pragma once
#include <QVariantMap>
#include <QtPlugin>

// Luch target-augmenter plugin interface — the ONLY thing crossing the
// ABI besides Qt types. Header-only (interface + IID); no Luch-internal
// structs.
//
// Contract:
//  - Manifest = sidecar <id>.json next to the .so:
//    {"id","title","description","priority"}.
//  - User config = optional ~/.config/luch/plugins/<id>.json
//    ({"enabled": …} + plugin-specific keys); absent file = enabled,
//    empty settings. Passed once via init() after load.
//  - augment() receives the original target as a map:
//      {"kind": "url"|"htmlfile", "url": <urlForm>, "raw": <raw arg>}
//    and priorSlices = {pluginId: slice, …} from earlier plugins
//    (read-only).
//  - Returns this plugin's slice. Reserved key: "url" — the outcome
//    URL, ONLY when the plugin produces one. All other keys
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
    virtual QVariantMap augment(const QVariantMap &target,
                                const QVariantMap &priorSlices) const = 0;
};
#define LuchTargetAugmenter_iid "app.luch.TargetAugmenter/1.0"
Q_DECLARE_INTERFACE(LuchTargetAugmenter, LuchTargetAugmenter_iid)
