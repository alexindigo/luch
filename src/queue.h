#pragma once

#include <QObject>

#include "target.h"

namespace Luch {

class TargetQueue : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int cursor READ cursor NOTIFY cursorChanged)
    Q_PROPERTY(QString currentRaw READ currentRaw NOTIFY currentChanged)
    Q_PROPERTY(QString currentScheme READ currentScheme NOTIFY currentChanged)
    Q_PROPERTY(QString currentHostOrDir READ currentHostOrDir NOTIFY currentChanged)
    Q_PROPERTY(QString currentMiddle READ currentMiddle NOTIFY currentChanged)
    Q_PROPERTY(QString currentTail READ currentTail NOTIFY currentChanged)
    // Full payload of the current target: {"original": targetMap,
    // "url": <effective>, "detected": […], "trace": […]}. Slice
    // presence is the stage state — a plugin with no trace entry has
    // not delivered yet.
    Q_PROPERTY(QVariantMap currentPayload READ currentPayload NOTIFY currentChanged)

public:
    explicit TargetQueue(QObject *parent = nullptr);

    int count() const;
    int cursor() const;

    void append(const Target &target);
    Q_INVOKABLE bool removeCurrent();
    Q_INVOKABLE void moveCursor(int delta);
    Q_INVOKABLE void clear();

    const Target *current() const;
    QString currentRaw() const;
    QString currentScheme() const;
    QString currentHostOrDir() const;
    QString currentMiddle() const;
    QString currentTail() const;
    QVariantMap currentPayload() const;

    // Reserved for async plugins: a late slice from plugin `id`
    // arrives here — appended as the plugin's next trace entry
    // {plugin, iteration, data}; main-level "url" and "detected"
    // update accordingly, then payloadChanged fires.
    Q_INVOKABLE void patchSlice(const QString &id, const QVariantMap &slice);

Q_SIGNALS:
    void countChanged();
    void cursorChanged();
    void currentChanged();
    void payloadChanged();

private:
    QList<Target> m_items;
    int m_cursor = 0;
};

} // namespace Luch
