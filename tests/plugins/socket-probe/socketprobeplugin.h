#pragma once

#include <QObject>
#include <QtPlugin>

#include "luchaugmenter.h"

// Verification probe (never installed): attempts an AF_INET socket and
// an AF_UNIX socket from augment() and reports the outcomes, proving
// the offline worker's seccomp sandbox blocks INET while allowing
// local IPC.
class SocketProbePlugin : public QObject, public LuchTargetAugmenter
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID LuchTargetAugmenter_iid)
    Q_INTERFACES(LuchTargetAugmenter)

public:
    QVariantMap augment(const QVariantMap &chainState,
                        const QVariantMap &priorSlices) const override;
};
