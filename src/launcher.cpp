#include "launcher.h"

#include <QDir>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

#include <vector>

#include "xdgactivation.h"
#include "dbustransport.h"
#include "chromiumsocket.h"

namespace Luch {

namespace {

QString targetForm(const Target &target, const QString &presentedUrl)
{
    if (!presentedUrl.isEmpty())
        return presentedUrl;
    return target.kind == Target::HtmlFile && !target.urlForm.isEmpty()
               ? target.urlForm
               : target.raw;
}

QString expandFieldCodes(const QString &token, const Target &target,
                         const QString &presentedUrl, bool &empty)
{
    QString out;
    out.reserve(token.size());
    for (int i = 0; i < token.size(); ++i) {
        const QChar ch = token.at(i);
        if (ch != QLatin1Char('%') || i + 1 >= token.size()) {
            out += ch;
            continue;
        }
        const QChar code = token.at(++i);
        switch (code.unicode()) {
        case 'u':
        case 'U':
            out += targetForm(target, presentedUrl);
            break;
        case 'f':
        case 'F':
            if (target.kind == Target::HtmlFile)
                out += target.path;
            break;
        case '%':
            out += QLatin1Char('%');
            break;
        case 'd':
        case 'D':
        case 'n':
        case 'N':
        case 'i':
        case 'c':
        case 'k':
        case 'v':
        case 'm':
            break;
        default:
            out += QLatin1Char('%');
            out += code;
            break;
        }
    }
    empty = out.isEmpty();
    return out;
}

} // namespace

Launcher::Launcher(QObject *parent)
    : QObject(parent)
    , m_activationTimer(new QTimer(this))
{
    m_activationTimer->setSingleShot(true);
    m_activationTimer->setInterval(200);
    connect(m_activationTimer, &QTimer::timeout, this,
            [this] { proceedPending(QString()); });
}

void Launcher::setTarget(const Target &target)
{
    m_target = target;
}

void Launcher::setPresentedUrl(const QString &url)
{
    m_presentedUrl = url;
}

void Launcher::setShellIntegrationRestore(bool wasSet, const QString &value)
{
    m_shellIntegrationWasSet = wasSet;
    m_shellIntegrationValue = value;
}

QProcessEnvironment Launcher::childEnvironment(const QString &token) const
{
    // LayerShellQt's useLayerShell() exports QT_WAYLAND_SHELL_INTEGRATION
    // into our own environment; launched Qt apps would inherit it and die.
    // Give children the pre-useLayerShell state, not blanket removal.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (m_shellIntegrationWasSet)
        env.insert(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"),
                   m_shellIntegrationValue);
    else
        env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    if (!token.isEmpty())
        env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);
    return env;
}

void Launcher::setActivationSource(XdgActivationTokenRequester *requester,
                                   QWindow *window)
{
    m_activationRequester = requester;
    m_activationWindow = window;
    if (!requester)
        return;
    connect(requester, &XdgActivationTokenRequester::tokenReady, this,
            [this](const QString &token) { proceedPending(token); });
    connect(requester, &XdgActivationTokenRequester::failed, this,
            [this] { proceedPending(QString()); });
}

void Launcher::setDbusTransport(DbusTransport *transport)
{
    m_dbusTransport = transport;
}

bool Launcher::launch(const QString &execLine, const QString &desktopId)
{
    const QStringList tokens = QProcess::splitCommand(execLine);
    if (tokens.isEmpty()) {
        Q_EMIT launchFailed(QStringLiteral("Empty Exec line"));
        return false;
    }

    QString pinnedProfile;
    for (int i = 0; i < tokens.size(); ++i) {
        const QString token = tokens.at(i);
        if ((token == QLatin1String("-P") || token == QLatin1String("--profile"))
            && i + 1 < tokens.size()) {
            pinnedProfile = tokens.at(i + 1);
            break;
        }
    }

    QStringList expanded;
    expanded.reserve(tokens.size());
    for (const QString &token : tokens) {
        bool empty = false;
        const QString arg =
            expandFieldCodes(token, m_target, m_presentedUrl, empty);
        if (!empty)
            expanded.append(arg);
    }

    if (expanded.isEmpty()) {
        Q_EMIT launchFailed(QStringLiteral("Exec line has no program"));
        return false;
    }

    const QString program = expanded.takeFirst();

    if (!m_activationRequester || !m_activationWindow) {
        doLaunch(program, expanded, QString());
        return true;
    }

    m_pendingProgram = program;
    m_pendingArgs = expanded;
    m_pendingDesktopId = desktopId;
    m_pendingProfile = pinnedProfile;
    m_proceeded = false;
    m_pendingActivation = true;
    m_activationTimer->start();
    m_activationRequester->requestToken(m_activationWindow);
    return true;
}

void Launcher::proceedPending(const QString &token)
{
    if (!m_pendingActivation || m_proceeded)
        return;
    m_proceeded = true;
    m_pendingActivation = false;
    m_activationTimer->stop();

    if (tryTransports(token)) {
        Q_EMIT launched();
        return;
    }
    doLaunch(m_pendingProgram, m_pendingArgs, token);
}

bool Launcher::tryTransports(const QString &token)
{
    if (m_pendingDesktopId.isEmpty())
        return false;

    const QString url = targetForm(m_target, m_presentedUrl);

    QString mozillaApp;
    static const char *mozillaFamily[] = {"firefox", "thunderbird",
                                          "librewolf"};
    for (const char *app : mozillaFamily) {
        if (m_pendingDesktopId.contains(QLatin1String(app))) {
            mozillaApp = QLatin1String(app);
            break;
        }
    }

    if (m_dbusTransport) {
        if (m_dbusTransport->openUrl(mozillaApp, m_pendingProfile,
                                     m_pendingDesktopId, url, token)) {
            qCDebug(luchTransport) << "dbus transport routed target";
            return true;
        }
        if (!mozillaApp.isEmpty())
            return false;
    } else if (!mozillaApp.isEmpty()) {
        return false;
    }

    static const char *chromiumFamily[] = {"chromium", "chrome", "brave"};
    bool isChromiumFamily = false;
    for (const char *app : chromiumFamily) {
        if (m_pendingDesktopId.contains(QLatin1String(app))) {
            isChromiumFamily = true;
            break;
        }
    }
    if (!isChromiumFamily)
        return false;

    const QStringList dirs = {
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/chromium"),
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/google-chrome"),
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/BraveSoftware/Brave-Browser"),
    };
    std::vector<std::string> userDataDirs;
    for (const QString &dir : dirs)
        userDataDirs.push_back(dir.toStdString());

    const QStringList argv = [this, &url, &token] {
        QStringList out = {m_pendingProgram};
        if (!token.isEmpty())
            out << QStringLiteral("--xdg-activation-token=%1").arg(token);
        out << url;
        return out;
    }();
    std::vector<std::string> rawArgv;
    for (const QString &arg : argv)
        rawArgv.push_back(arg.toStdString());

    const ChromiumSocket::Result result = ChromiumSocket::notifyRunningInstance(
        userDataDirs, QDir::currentPath().toStdString(), rawArgv, 1000);
    if (result != ChromiumSocket::Result::Acked) {
        qCDebug(luchTransport)
            << "chromium socket transport: no ACK, falling back to CLI";
        return false;
    }
    qCDebug(luchTransport) << "chromium socket transport routed target";
    return true;
}

void Launcher::doLaunch(const QString &program, const QStringList &args,
                        const QString &token)
{
    // Detach the child's stdio from ours: launched browsers otherwise
    // inherit the terminal (polluting the prompt and holding the tty).
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.setStandardErrorFile(QProcess::nullDevice());
    proc.setProcessEnvironment(childEnvironment(token));
    if (!proc.startDetached()) {
        Q_EMIT launchFailed(
            QStringLiteral("Failed to start %1").arg(program));
        return;
    }

    Q_EMIT launched();
}

void Launcher::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    Q_EMIT copied();
    // Wayland selections die with their owner; wl-copy daemonizes and
    // keeps the selection alive after we exit. Best effort only.
    const QString wlCopy =
        QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
    if (!wlCopy.isEmpty()) {
        QProcess proc;
        proc.setProgram(wlCopy);
        proc.setArguments({text});
        proc.setStandardInputFile(QProcess::nullDevice());
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        proc.setProcessEnvironment(childEnvironment(QString()));
        proc.startDetached();
    }
}

} // namespace Luch
