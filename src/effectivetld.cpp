#include "effectivetld.h"

#include <QFile>
#include <QUrl>

namespace Luch {

bool EffectiveTld::load(const QString &pslPath)
{
    QFile file(pslPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray all = file.readAll();
    const QByteArray beginMarker = "// ===BEGIN ICANN DOMAINS===";
    const QByteArray endMarker = "// ===END ICANN DOMAINS===";
    const int begin = all.indexOf(beginMarker);
    const int end = all.indexOf(endMarker);
    if (begin < 0 || end < 0 || end <= begin)
        return false;

    const QByteArray section = all.mid(begin + beginMarker.size(),
                                       end - begin - beginMarker.size());
    const QList<QByteArray> lines = section.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith("//"))
            continue;
        if (line.startsWith('!'))
            m_exception.insert(QString::fromUtf8(line.mid(1)).toLower());
        else if (line.startsWith("*."))
            m_wildcard.insert(QString::fromUtf8(line.mid(2)).toLower());
        else
            m_exact.insert(QString::fromUtf8(line).toLower());
    }
    m_loaded = true;
    return true;
}

QString EffectiveTld::eTLDPlusOne(const QString &host) const
{
    // Mirrors Chromium: trailing dot removed before matching, IDN handled
    // in punycode, case-insensitive.
    QString h = host;
    while (h.endsWith(QLatin1Char('.')))
        h.chop(1);
    h = QUrl::toAce(h.toLower());
    if (h.isEmpty())
        return QString();

    const QStringList labels = h.split(QLatin1Char('.'));
    const int n = labels.size();
    if (n < 2)
        return QString();

    // Exception rules take precedence over everything: if any suffix of
    // the host matches one, the public suffix is that rule minus its
    // leftmost label — i.e. the eTLD starts one label further right, so
    // the registrable domain keeps one more label than a plain match.
    for (int i = 0; i < n; ++i) {
        const QString candidate = labels.mid(i).join(QLatin1Char('.'));
        if (m_exception.contains(candidate))
            return labels.mid(i).join(QLatin1Char('.'));
    }

    // Otherwise the prevailing rule is the longest match (exact or
    // wildcard; a wildcard consumes exactly one extra label).
    int etldLabels = 0;
    for (int i = 0; i < n; ++i) {
        const QStringList candidateLabels = labels.mid(i);
        const QString candidate = candidateLabels.join(QLatin1Char('.'));
        if (m_exact.contains(candidate))
            etldLabels = qMax(etldLabels, candidateLabels.size());
        // "*.x" matches candidate iff candidate[1:] == "x" rule body.
        if (candidateLabels.size() >= 2
            && m_wildcard.contains(
                candidateLabels.mid(1).join(QLatin1Char('.'))))
            etldLabels = qMax(etldLabels, candidateLabels.size());
    }
    // "If no rules match, the prevailing rule is '*'": the public suffix
    // is the last label.
    if (etldLabels == 0)
        etldLabels = 1;

    // The host itself is a public suffix → no registrable domain.
    if (etldLabels >= n)
        return QString();
    return labels.mid(n - etldLabels - 1).join(QLatin1Char('.'));
}

} // namespace Luch
