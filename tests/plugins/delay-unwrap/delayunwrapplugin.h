#pragma once

#include <QObject>
#include <QtPlugin>

#include "luchaugmenter.h"

// Verification plugin (never installed): declares inet and sleeps in
// augment() to simulate a slow online lookup. Runs queued on the inet
// worker, so the picker must appear before its slice lands via
// patchSlice — the artificial-delay gate for the async contract.
class DelayUnwrapPlugin : public QObject, public LuchTargetAugmenter
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID LuchTargetAugmenter_iid)
    Q_INTERFACES(LuchTargetAugmenter)

public:
    QVariantMap augment(const QVariantMap &chainState,
                        const QVariantMap &priorSlices) const override;
};
