#include "urltools.h"

#include <QDir>
#include <QStandardPaths>

namespace Luch {

UrlTools::UrlTools(QObject *parent)
    : QObject(parent)
{
    // PSL dir: $LUCH_LISTS_DIR → ~/.config/luch/lists/ → installed
    // (same resolution as the list-consuming plugins).
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
    if (!m_tld.load(dir + QStringLiteral("/public_suffix_list.dat")))
        qWarning() << "urltools: public suffix list failed to load from"
                   << dir;
}

QString UrlTools::eTLDPlusOne(const QString &host) const
{
    return m_tld.eTLDPlusOne(host);
}

} // namespace Luch
