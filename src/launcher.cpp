#include "launcher.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QStandardPaths>

namespace Luch {

namespace {

QString expandFieldCodes(const QString &token, const QString &url, bool &empty)
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
            out += url;
            break;
        case '%':
            out += QLatin1Char('%');
            break;
        case 'f':
        case 'F':
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
{
}

bool Launcher::launch(const QString &execLine, const QString &url)
{
    const QStringList tokens = QProcess::splitCommand(execLine);
    if (tokens.isEmpty()) {
        Q_EMIT launchFailed(QStringLiteral("Empty Exec line"));
        return false;
    }

    QStringList expanded;
    expanded.reserve(tokens.size());
    for (const QString &token : tokens) {
        bool empty = false;
        const QString arg = expandFieldCodes(token, url, empty);
        if (!empty)
            expanded.append(arg);
    }

    if (expanded.isEmpty()) {
        Q_EMIT launchFailed(QStringLiteral("Exec line has no program"));
        return false;
    }

    const QString program = expanded.takeFirst();
    if (!QProcess::startDetached(program, expanded)) {
        Q_EMIT launchFailed(
            QStringLiteral("Failed to start %1").arg(program));
        return false;
    }

    Q_EMIT launched();
    return true;
}

void Launcher::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    // Wayland selections die with their owner; wl-copy daemonizes and
    // keeps the selection alive after we exit. Best effort only.
    const QString wlCopy =
        QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
    if (!wlCopy.isEmpty())
        QProcess::startDetached(wlCopy, {text});
}

} // namespace Luch
