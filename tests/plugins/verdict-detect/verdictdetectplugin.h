#pragma once

#include <QObject>
#include <QtPlugin>

#include "luchaugmenter.h"

// Verification plugin (never installed): Detect-stage verdict slice —
// drives the red verdict affordances (verdict dot, warning line,
// dissection auto-show) in the VM gates.
class VerdictDetectPlugin : public QObject, public LuchTargetAugmenter
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID LuchTargetAugmenter_iid)
    Q_INTERFACES(LuchTargetAugmenter)

public:
    QVariantMap augment(const QVariantMap &chainState,
                        const QVariantMap &priorSlices) const override;
};
