#include "debounceplugin.h"

#include <QDir>
#include <QStandardPaths>
#include <QUrl>

void DebouncePlugin::init(const QVariantMap &userConfig)
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
        qWarning() << "debounce: some lists failed to load from" << dir;
}

QVariantMap DebouncePlugin::augment(const QVariantMap &chainState,
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

    const Luch::DebounceResult result = m_cleaner.unwrap(url);
    QVariantMap slice{
        {QStringLiteral("debounced"), result.changed},
    };
    // Wrapper-pattern flag — emitted even when no rule fires; the
    // pipeline's online fallback (future plugin) triggers on it.
    if (result.shortener)
        slice.insert(QStringLiteral("shortener"), true);
    if (result.changed) {
        slice.insert(QStringLiteral("url"), result.url);
        slice.insert(QStringLiteral("debouncedFrom"),
                     result.debouncedFrom);
    }
    return slice;
}
