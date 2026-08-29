#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QStringList>

#include "target.h"

class QTimer;
class QWindow;
class XdgActivationTokenRequester;

namespace Luch {

class DbusTransport;

class Launcher : public QObject
{
    Q_OBJECT

public:
    explicit Launcher(QObject *parent = nullptr);

    void setTarget(const Target &target);
    void setShellIntegrationRestore(bool wasSet, const QString &value);
    void setActivationSource(XdgActivationTokenRequester *requester,
                             QWindow *window);
    void setDbusTransport(DbusTransport *transport);

    Q_INVOKABLE bool launch(const QString &execLine,
                            const QString &desktopId = QString());
    Q_INVOKABLE void copyToClipboard(const QString &text);

Q_SIGNALS:
    void launched();
    void launchFailed(const QString &message);
    void copied();

private:
    void proceedPending(const QString &token);
    void doLaunch(const QString &program, const QStringList &args,
                  const QString &token);
    bool tryTransports(const QString &token);
    QProcessEnvironment childEnvironment(const QString &token) const;

    Target m_target;
    bool m_shellIntegrationWasSet = false;
    QString m_shellIntegrationValue;
    XdgActivationTokenRequester *m_activationRequester = nullptr;
    QWindow *m_activationWindow = nullptr;
    DbusTransport *m_dbusTransport = nullptr;
    QTimer *m_activationTimer = nullptr;
    QString m_pendingProgram;
    QStringList m_pendingArgs;
    QString m_pendingDesktopId;
    QString m_pendingProfile;
    bool m_pendingActivation = false;
    bool m_proceeded = false;
};

} // namespace Luch
