#include "verdictdetectplugin.h"

#include <QUrl>
#include <QVariantMap>

QVariantMap VerdictDetectPlugin::augment(
    const QVariantMap &chainState,
    const QVariantMap &priorSlices) const
{
    Q_UNUSED(priorSlices);

    if (chainState.value(QStringLiteral("kind")).toString()
        != QLatin1String("url"))
        return {};
    const QUrl url(chainState.value(QStringLiteral("url")).toString());
    if (url.scheme() != QLatin1String("http")
        && url.scheme() != QLatin1String("https"))
        return {};

    // Terminal evaluator: danger verdict on the working URL, no url
    // key — Detect iterations never change the working URL.
    return {{QStringLiteral("verdict"), QStringLiteral("malicious")},
            {QStringLiteral("source"), QStringLiteral("verdict-test")}};
}
