#include "queue.h"

namespace Luch {

TargetQueue::TargetQueue(QObject *parent)
    : QObject(parent)
{
}

int TargetQueue::count() const
{
    return m_items.size();
}

int TargetQueue::cursor() const
{
    return m_cursor;
}

void TargetQueue::append(const Target &target)
{
    m_items.append(target);
    Q_EMIT countChanged();
}

bool TargetQueue::removeCurrent()
{
    if (m_cursor < 0 || m_cursor >= m_items.size())
        return false;
    m_items.removeAt(m_cursor);
    if (m_cursor >= m_items.size())
        m_cursor = qMax(0, m_items.size() - 1);
    Q_EMIT countChanged();
    Q_EMIT cursorChanged();
    Q_EMIT currentChanged();
    return true;
}

void TargetQueue::moveCursor(int delta)
{
    const int next = qBound(0, m_cursor + delta, m_items.size() - 1);
    if (next == m_cursor)
        return;
    m_cursor = next;
    Q_EMIT cursorChanged();
    Q_EMIT currentChanged();
}

void TargetQueue::clear()
{
    m_items.clear();
    m_cursor = 0;
    Q_EMIT countChanged();
    Q_EMIT cursorChanged();
    Q_EMIT currentChanged();
}

const Target *TargetQueue::current() const
{
    if (m_cursor < 0 || m_cursor >= m_items.size())
        return nullptr;
    return &m_items.at(m_cursor);
}

QString TargetQueue::currentRaw() const
{
    const Target *t = current();
    return t ? t->raw : QString();
}

QString TargetQueue::currentScheme() const
{
    const Target *t = current();
    return t ? t->scheme : QString();
}

QString TargetQueue::currentHostOrDir() const
{
    const Target *t = current();
    return t ? t->hostOrDir : QString();
}

QString TargetQueue::currentMiddle() const
{
    const Target *t = current();
    return t ? t->middle : QString();
}

QString TargetQueue::currentTail() const
{
    const Target *t = current();
    return t ? t->tail : QString();
}

QVariantMap TargetQueue::currentPayload() const
{
    const Target *t = current();
    return t ? t->pluginData : QVariantMap{};
}

void TargetQueue::patchSlice(const QString &id, const QVariantMap &slice)
{
    if (m_cursor < 0 || m_cursor >= m_items.size())
        return;
    QVariantMap payload = m_items[m_cursor].pluginData;

    QVariantList trace = payload.value(QStringLiteral("trace")).toList();
    // Iteration number: this plugin's existing entries + 1.
    int iteration = 1;
    for (const QVariant &entry : trace) {
        const QVariantMap e = entry.toMap();
        if (e.value(QStringLiteral("plugin")).toString() == id)
            iteration = e.value(QStringLiteral("iteration")).toInt() + 1;
    }
    trace.append(QVariantMap{{QStringLiteral("plugin"), id},
                             {QStringLiteral("iteration"), iteration},
                             {QStringLiteral("data"), slice}});
    payload.insert(QStringLiteral("trace"), trace);

    // A late slice that produced a URL moves the effective URL.
    const QString out = slice.value(QStringLiteral("url")).toString();
    if (!out.isEmpty())
        payload.insert(QStringLiteral("url"), out);

    if (!slice.value(QStringLiteral("verdict")).isNull()) {
        QVariantList detected =
            payload.value(QStringLiteral("detected")).toList();
        detected.append(QVariantMap{{QStringLiteral("plugin"), id},
                                    {QStringLiteral("verdict"),
                                     slice.value(QStringLiteral("verdict"))},
                                    {QStringLiteral("source"),
                                     slice.value(QStringLiteral("source"))}});
        payload.insert(QStringLiteral("detected"), detected);
    }

    m_items[m_cursor].pluginData = payload;
    Q_EMIT payloadChanged();
}

} // namespace Luch
