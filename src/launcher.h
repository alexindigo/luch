#pragma once

#include <QObject>

namespace Luch {

class Launcher : public QObject
{
    Q_OBJECT

public:
    explicit Launcher(QObject *parent = nullptr);

    Q_INVOKABLE bool launch(const QString &execLine, const QString &url);
    Q_INVOKABLE void copyToClipboard(const QString &text);

Q_SIGNALS:
    void launched();
    void launchFailed(const QString &message);
};

} // namespace Luch
