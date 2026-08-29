#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(luchTransport)

namespace Luch {

// D-Bus clients for talking to RUNNING browser instances:
//  - Mozilla remote: org.mozilla.<app>.<base64 profile>, OpenURL(ay)
//    (payload = the X-remote command-line blob per
//    toolkit/components/remote/nsUnixRemoteServer.cpp)
//  - org.freedesktop.Application.Open(as uris, a{sv} platform-data)
// Both calls block with a nested event loop (QDBus::BlockWithGui), so the
// picker stays responsive and the result is known synchronously.
class DbusTransport : public QObject
{
    Q_OBJECT

public:
    explicit DbusTransport(QObject *parent = nullptr);

    // mozillaApp: family app name ("firefox", "thunderbird", …) or empty
    // to skip the Mozilla path. pinnedProfile: `-P`/`--profile` value from
    // the Exec line, used to disambiguate profile bus names; empty means
    // any profile. desktopId: entry id used for the FDO Application path.
    // Returns true when a transport accepted the target.
    bool openUrl(const QString &mozillaApp, const QString &pinnedProfile,
                 const QString &desktopId, const QString &url,
                 const QString &activationToken);
};

} // namespace Luch
