#include "target.h"

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>

namespace Luch {

namespace {

bool makeUrlTarget(const QString &raw, const QUrl &url, Target &target)
{
    target = Target();
    target.kind = Target::Url;
    target.raw = raw;
    target.urlForm = raw;
    target.scheme = url.scheme() + QLatin1String("://");

    QString host = url.host();
    const int port = url.port();
    if (port != -1) {
        const int defaultPort =
            url.scheme() == QLatin1String("https") ? 443 : 80;
        if (port != defaultPort)
            host += QLatin1Char(':') + QString::number(port);
    }
    target.hostOrDir = host;

    const QString path = url.path();
    const int lastSlash = path.lastIndexOf(QLatin1Char('/'));
    target.middle = lastSlash > 0 ? path.left(lastSlash) : QString();
    target.tail = path.isEmpty()
                      ? QString()
                      : QLatin1Char('/') + path.mid(lastSlash + 1);
    if (!url.query().isEmpty())
        target.tail += QLatin1Char('?') + url.query();
    if (url.hasFragment())
        target.tail += QLatin1Char('#') + url.fragment();
    return true;
}

bool makeFileTarget(const QString &raw, const QString &path, Target &target,
                    QString *errorMessage)
{
    const QMimeDatabase mimeDb;
    const QMimeType mime = mimeDb.mimeTypeForFile(path);
    if (!mime.inherits(QLatin1String("text/html"))
        && !mime.inherits(QLatin1String("application/xhtml+xml"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                                "luch: only HTML files are supported for now;"
                                " %1 is %2")
                                .arg(path, mime.name());
        }
        return false;
    }

    target = Target();
    target.kind = Target::HtmlFile;
    target.raw = raw;
    target.path = QDir::cleanPath(path);
    target.urlForm = QUrl::fromLocalFile(target.path).toString();
    target.scheme = QStringLiteral("file://");

    const int lastSlash = target.path.lastIndexOf(QLatin1Char('/'));
    target.tail = QLatin1Char('/') + target.path.mid(lastSlash + 1);
    const QString dir = target.path.left(lastSlash);
    if (dir.isEmpty() || dir == QLatin1String("/")) {
        target.hostOrDir = QString();
        target.middle = QString();
    } else {
        const int secondSlash = dir.indexOf(QLatin1Char('/'), 1);
        if (secondSlash == -1) {
            target.hostOrDir = dir;
            target.middle = QString();
        } else {
            target.hostOrDir = dir.left(secondSlash);
            target.middle = dir.mid(secondSlash);
        }
    }
    return true;
}

} // namespace

QVariantMap Target::toMap() const
{
    return {{QStringLiteral("kind"), kind == Url ? QStringLiteral("url")
                                                : QStringLiteral("htmlfile")},
            {QStringLiteral("url"), urlForm},
            {QStringLiteral("raw"), raw}};
}

bool Target::parse(const QString &argument, Target &target,
                   QString *errorMessage)
{
    if (argument.startsWith(QLatin1String("file://"))) {
        const QUrl uri(argument);
        const QString path = uri.toLocalFile();
        if (!uri.isValid() || path.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("luch: invalid file:// URI: %1")
                                    .arg(argument);
            }
            return false;
        }
        return makeFileTarget(argument, path, target, errorMessage);
    }

    const QUrl url(argument);
    if (url.isValid()
        && (url.scheme() == QLatin1String("http")
            || url.scheme() == QLatin1String("https"))) {
        return makeUrlTarget(argument, url, target);
    }

    const QFileInfo info(argument);
    if (info.exists()) {
        return makeFileTarget(argument, info.absoluteFilePath(), target,
                              errorMessage);
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("luch: not a routable http(s) URL: %1")
                            .arg(argument);
    }
    return false;
}

} // namespace Luch
