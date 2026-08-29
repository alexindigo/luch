#include "singleinstance.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace Luch {

namespace {
const QString kBusName = QStringLiteral("app.luch");
const QString kObjectPath = QStringLiteral("/app/luch");
const QString kSocketName = QStringLiteral("luch.sock");
} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

SingleInstance::~SingleInstance()
{
    if (m_server)
        m_server->close();
}

bool SingleInstance::tryClaim(const QString &target)
{
    if (registerDbus()) {
        m_useDbus = true;
        return true;
    }

    // Name taken (or bus failure) — try forwarding to the owner.
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            kBusName, kObjectPath,
            QStringLiteral("org.freedesktop.Application"),
            QStringLiteral("Open"));
        call << QStringList{target} << QVariantMap{};
        const QDBusMessage reply =
            QDBusConnection::sessionBus().call(call, QDBus::Block);
        if (reply.type() != QDBusMessage::ErrorMessage)
            return false;
    }

    // No session bus owner claim — fall back to a local socket.
    if (registerSocket())
        return true;

    // Socket path busy: forward over the socket to the listening owner.
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
        + QStringLiteral("/") + kSocketName;
    QLocalSocket socket;
    socket.connectToServer(path);
    if (socket.waitForConnected(1000)) {
        socket.write(target.toUtf8() + '\n');
        socket.flush();
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return false;
    }

    // Stale socket file with nobody listening: clean it and claim.
    QLocalServer::removeServer(path);
    if (registerSocket())
        return true;

    // Nothing worked; run standalone rather than drop the target.
    return true;
}

bool SingleInstance::registerDbus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kBusName))
        return false;
    if (!bus.registerObject(kObjectPath, this,
                            QDBusConnection::ExportAllSlots)) {
        bus.unregisterService(kBusName);
        return false;
    }
    return true;
}

bool SingleInstance::registerSocket()
{
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
        + QStringLiteral("/") + kSocketName;

    m_server = new QLocalServer(this);
    if (!m_server->listen(path)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                while (client->canReadLine()) {
                    const QString line =
                        QString::fromUtf8(client->readLine()).trimmed();
                    if (!line.isEmpty())
                        Q_EMIT targetArrived(line);
                }
            });
            connect(client, &QLocalSocket::disconnected, client,
                    &QLocalSocket::deleteLater);
        }
    });
    return true;
}

void SingleInstance::Open(const QStringList &uris,
                          const QVariantMap &platformData)
{
    Q_UNUSED(platformData);
    for (const QString &uri : uris)
        Q_EMIT targetArrived(uri);
}

} // namespace Luch
