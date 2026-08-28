#pragma once

#include <QObject>

#include "target.h"

namespace Luch {

class Launcher : public QObject
{
    Q_OBJECT

public:
    explicit Launcher(QObject *parent = nullptr);

    void setTarget(const Target &target);

    Q_INVOKABLE bool launch(const QString &execLine);
    Q_INVOKABLE void copyToClipboard(const QString &text);

Q_SIGNALS:
    void launched();
    void launchFailed(const QString &message);

private:
    Target m_target;
};

} // namespace Luch
