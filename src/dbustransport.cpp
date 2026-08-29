#include "dbustransport.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QtEndian>

Q_LOGGING_CATEGORY(luchTransport, "luch.transport")

namespace {

QStringList registeredBusNames()
{
    const QDBusReply<QStringList> names =
        QDBusConnection::sessionBus().interface()->registeredServiceNames();
    return names.isValid() ? names.value() : QStringList();
}

// Firefox/OpenURL payload (nsUnixRemoteServer::HandleCommandLine):
//   [argc:LE32][offset argv0:LE32]…[offset argvN:LE32]
//   <workingDir>\0<argv0>\0…\0<argvN>\0
// Offsets are byte positions from the buffer start.
QByteArray mozillaCommandLinePayload(const QString &workingDir,
                                    const QStringList &argv)
{
    QByteArray buf;
    const auto appendU32 = [&buf](quint32 value) {
        char raw[4];
        qToLittleEndian(value, raw);
        buf.append(raw, 4);
    };

    const quint32 argc = argv.size();
    appendU32(argc);

    quint32 position = 4u * (argc + 1u);
    position += workingDir.toUtf8().size() + 1;
    for (const QString &arg : argv) {
        appendU32(position);
        position += arg.toUtf8().size() + 1;
    }

    buf.append(workingDir.toUtf8());
    buf.append('\0');
    for (const QString &arg : argv) {
        buf.append(arg.toUtf8());
        buf.append('\0');
    }
    return buf;
}

bool tryMozilla(const QString &app, const QString &pinnedProfile,
                const QString &url, const QString &activationToken)
{
    const QString prefix = QStringLiteral("org.mozilla.%1.").arg(app);
    QStringList candidates;
    for (const QString &name : registeredBusNames()) {
        if (!name.startsWith(prefix))
            continue;
        if (!pinnedProfile.isEmpty()) {
            const QByteArray decoded =
                QByteArray::fromBase64(name.mid(prefix.size()).toUtf8());
            if (!QString::fromUtf8(decoded).endsWith(
                    QLatin1Char('/') + pinnedProfile)) {
                continue;
            }
        }
        candidates << name;
    }
    if (candidates.isEmpty()) {
        qCInfo(luchTransport) << "mozilla transport: no bus name for"
                              << app << "profile" << pinnedProfile;
        return false;
    }

    const QString service = candidates.first();

    // Firefox's own forwarder delivers the activation token in-band:
    // XDG_ACTIVATION_TOKEN env → " STARTUP_TOKEN=" appended to argv[0]
    // (nsAppRunner → nsDBusRemoteClient → nsGTKToolkit →
    // xdg_activation_v1.activate in the receiving instance).
    QString argv0 = app;
    if (!activationToken.isEmpty())
        argv0 += QStringLiteral(" STARTUP_TOKEN=%1").arg(activationToken);

    QDBusMessage call = QDBusMessage::createMethodCall(
        service, QStringLiteral("/org/mozilla/%1/Remote").arg(app),
        QStringLiteral("org.mozilla.%1").arg(app), QStringLiteral("OpenURL"));
    call << mozillaCommandLinePayload(QDir::currentPath(), {argv0, url});
    const QDBusMessage reply =
        QDBusConnection::sessionBus().call(call, QDBus::BlockWithGui);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCInfo(luchTransport) << "mozilla transport: OpenURL failed:"
                              << reply.errorMessage();
        return false;
    }
    qCInfo(luchTransport) << "mozilla transport: OpenURL accepted by"
                          << service;
    return true;
}

bool tryFreeDesktop(const QString &desktopId, const QString &url,
                    const QString &activationToken)
{
    if (!desktopId.contains(QLatin1Char('.')))
        return false;
    if (!registeredBusNames().contains(desktopId)) {
        qCInfo(luchTransport) << "fdo transport: no bus name" << desktopId;
        return false;
    }

    const QString objectPath =
        QStringLiteral("/") + desktopId.split(QLatin1Char('.')).join(
                                  QLatin1Char('/'));
    QDBusMessage call = QDBusMessage::createMethodCall(
        desktopId, objectPath, QStringLiteral("org.freedesktop.Application"),
        QStringLiteral("Open"));
    call << QStringList{url};
    QVariantMap platformData;
    if (!activationToken.isEmpty())
        platformData.insert(QStringLiteral("activation-token"),
                            activationToken);
    call << platformData;

    const QDBusMessage reply =
        QDBusConnection::sessionBus().call(call, QDBus::BlockWithGui);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCInfo(luchTransport) << "fdo transport: Open failed:"
                              << reply.errorMessage();
        return false;
    }
    qCInfo(luchTransport) << "fdo transport: Open accepted by" << desktopId;
    return true;
}

// GTK/GApplication family (GNOME Web etc.): org.gtk.Application.Activate
// with the app's startup context in platform-data — this is how the app's
// own second-instance CLI remotes (GApplication machinery). GNOME Web
// carries URIs under "ephy-shell-startup-context": a{iv} entry
// CTX_ARGUMENTS(2) → variant(as[uris]); mode 0 = new tab.
bool tryGtkApplication(const QString &desktopId, const QString &url,
                       const QString &activationToken)
{
    if (!desktopId.contains(QLatin1String("epiphany"), Qt::CaseInsensitive))
        return false;
    if (!registeredBusNames().contains(desktopId)) {
        qCInfo(luchTransport) << "gtk transport: no bus name" << desktopId;
        return false;
    }

    const QString objectPath =
        QStringLiteral("/") + desktopId.split(QLatin1Char('.')).join(
                                  QLatin1Char('/'));

    QVariantMap platformData;
    platformData.insert(QStringLiteral("cwd"),
                        QDir::currentPath().toUtf8() + '\0');
    if (!activationToken.isEmpty()) {
        // X11-interop key — Gtk merges X11 startup notifications and
        // xdg-activation (wayland-protocols x11-interoperation.rst).
        platformData.insert(QStringLiteral("desktop-startup-id"),
                            activationToken);
    }
    QDBusArgument ctx;
    ctx.beginMap(QMetaType::Int, qMetaTypeId<QDBusVariant>());
    ctx.beginMapEntry();
    ctx << 2; // CTX_ARGUMENTS
    ctx << QDBusVariant(QVariant::fromValue(QStringList{url}));
    ctx.endMapEntry();
    ctx.endMap();
    platformData.insert(QStringLiteral("ephy-shell-startup-context"),
                        QVariant::fromValue(ctx));

    QDBusMessage call = QDBusMessage::createMethodCall(
        desktopId, objectPath, QStringLiteral("org.gtk.Application"),
        QStringLiteral("Activate"));
    call << platformData;
    const QDBusMessage reply =
        QDBusConnection::sessionBus().call(call, QDBus::BlockWithGui);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCInfo(luchTransport) << "gtk transport: Activate failed:"
                              << reply.errorMessage();
        return false;
    }
    qCInfo(luchTransport) << "gtk transport: Activate accepted by"
                          << desktopId;
    return true;
}

} // namespace

namespace Luch {

DbusTransport::DbusTransport(QObject *parent)
    : QObject(parent)
{
}

bool DbusTransport::openUrl(const QString &mozillaApp,
                            const QString &pinnedProfile,
                            const QString &desktopId, const QString &url,
                            const QString &activationToken)
{
    if (!mozillaApp.isEmpty()
        && tryMozilla(mozillaApp, pinnedProfile, url, activationToken))
        return true;
    if (tryFreeDesktop(desktopId, url, activationToken))
        return true;
    if (tryGtkApplication(desktopId, url, activationToken))
        return true;
    return false;
}

} // namespace Luch
