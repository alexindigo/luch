#include "socketprobeplugin.h"

#include <QVariantMap>

#include <cerrno>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

QVariantMap SocketProbePlugin::augment(const QVariantMap &chainState,
                                       const QVariantMap &priorSlices) const
{
    Q_UNUSED(chainState);
    Q_UNUSED(priorSlices);

    QVariantMap slice;
    const int inetFd = socket(AF_INET, SOCK_STREAM, 0);
    if (inetFd < 0) {
        slice.insert(QStringLiteral("inetSocket"),
                     QStringLiteral("blocked"));
        slice.insert(QStringLiteral("inetErrno"), errno);
    } else {
        ::close(inetFd);
        slice.insert(QStringLiteral("inetSocket"),
                     QStringLiteral("connected"));
    }

    const int unixFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unixFd < 0) {
        slice.insert(QStringLiteral("unixSocket"),
                     QStringLiteral("failed"));
        slice.insert(QStringLiteral("unixErrno"), errno);
    } else {
        ::close(unixFd);
        slice.insert(QStringLiteral("unixSocket"), QStringLiteral("ok"));
    }
    return slice;
}
