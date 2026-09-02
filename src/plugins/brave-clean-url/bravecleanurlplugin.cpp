#include "bravecleanurlplugin.h"

#include <QDir>
#include <QStandardPaths>
#include <QUrl>

void BraveCleanUrlPlugin::init(const QVariantMap &userConfig)
{
    Q_UNUSED(userConfig); // no plugin-specific settings yet

    // Lists dir: $LUCH_LISTS_DIR → ~/.config/luch/lists/ → installed.
    QString dir = qEnvironmentVariable("LUCH_LISTS_DIR");
    if (dir.isEmpty()) {
        const QString userDir =
            QStandardPaths::writableLocation(
                QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/luch/lists");
        dir = QDir(userDir).exists()
                  ? userDir
                  : QStringLiteral(LUCH_INSTALLED_LISTS_DIR);
    }
    if (!m_cleaner.loadLists(dir))
        qWarning() << "brave-clean-url: some lists failed to load from"
                   << dir;
}

QVariantMap BraveCleanUrlPlugin::augment(
    const QVariantMap &chainState,
    const QVariantMap &priorSlices) const
{
    Q_UNUSED(priorSlices);

    // Only http(s) URL targets are actionable.
    if (chainState.value(QStringLiteral("kind")).toString()
        != QLatin1String("url"))
        return {};
    const QUrl url(chainState.value(QStringLiteral("url")).toString());
    if (url.scheme() != QLatin1String("http")
        && url.scheme() != QLatin1String("https"))
        return {};

    const Luch::CleanResult result = m_cleaner.strip(url);
    QVariantMap slice{
        {QStringLiteral("strippedCount"), result.strippedCount},
        {QStringLiteral("strippedParams"), result.strippedParams},
    };
    if (result.changed)
        slice.insert(QStringLiteral("url"), result.url);
    return slice;
}
