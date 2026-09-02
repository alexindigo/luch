#include "delayunwrapplugin.h"

#include <QThread>
#include <QUrl>
#include <QVariantMap>

QVariantMap DelayUnwrapPlugin::augment(
    const QVariantMap &chainState,
    const QVariantMap &priorSlices) const
{
    Q_UNUSED(priorSlices);

    if (chainState.value(QStringLiteral("kind")).toString()
        != QLatin1String("url"))
        return {};

    QThread::msleep(2000); // artificial slow lookup

    const QString working =
        chainState.value(QStringLiteral("url")).toString();
    // Idempotent: the fixpoint feeds the plugin its own output —
    // a URL already tagged is a noop, or the cap would spin.
    if (working.contains(QLatin1String("delayed=1")))
        return {};
    QVariantMap slice{{QStringLiteral("delayed"), true},
                      {QStringLiteral("delayMs"), 2000}};
    // A distinct variant URL so the late arrival materializes a pill.
    slice.insert(QStringLiteral("url"), working + QStringLiteral("?delayed=1"));
    return slice;
}
