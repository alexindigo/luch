#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QStringList>

#include "target.h"

class QTimer;
class QWindow;
class XdgActivationTokenRequester;

namespace Luch {

class Launcher : public QObject
{
    Q_OBJECT

public:
    explicit Launcher(QObject *parent = nullptr);

    void setTarget(const Target &target);
    void setActivationSource(XdgActivationTokenRequester *requester,
                             QWindow *window);

    Q_INVOKABLE bool launch(const QString &execLine);
    Q_INVOKABLE void copyToClipboard(const QString &text);

Q_SIGNALS:
    void launched();
    void launchFailed(const QString &message);
    void copied();

private:
    void proceedPending(const QString &token);
    void doLaunch(const QString &program, const QStringList &args,
                  const QString &token);

    Target m_target;
    XdgActivationTokenRequester *m_activationRequester = nullptr;
    QWindow *m_activationWindow = nullptr;
    QTimer *m_activationTimer = nullptr;
    QString m_pendingProgram;
    QStringList m_pendingArgs;
    bool m_pendingActivation = false;
    bool m_proceeded = false;
};

} // namespace Luch
