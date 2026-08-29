#pragma once

#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

namespace Luch {

class SingleInstance : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")

public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    // Returns true if we claimed the channel (owner path). Otherwise
    // forwards target to the running owner and returns false.
    bool tryClaim(const QString &target);

public Q_SLOTS:
    // org.freedesktop.Application
    void Open(const QStringList &uris, const QVariantMap &platformData);

Q_SIGNALS:
    void targetArrived(const QString &target);

private:
    bool registerDbus();
    bool registerSocket();

    bool m_useDbus = false;
    QLocalServer *m_server = nullptr;
};

} // namespace Luch
