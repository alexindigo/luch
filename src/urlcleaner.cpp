#include "urlcleaner.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace Luch {

namespace {

// ---------------------------------------------------------------- globs

// Escape regex specials except '*', which becomes ".*".
QString globEscape(const QString &in)
{
    QString out;
    out.reserve(in.size() * 2);
    for (const QChar c : in) {
        if (c == QLatin1Char('*'))
            out += QStringLiteral(".*");
        else if (QStringLiteral("\\^$.|?+()[]{}").contains(c))
            out += QLatin1Char('\\') + c;
        else
            out += c;
    }
    return out;
}

// Chromium URLPattern-style glob → anchored QRegularExpression.
// scheme "*" ≡ http|https; host prefix "*." ≡ bare domain or any
// subdomain depth; host "*" ≡ any host; scheme+host compare
// case-insensitively (inline (?i:…) group), path+query case-sensitively;
// full-string match.
QRegularExpression globToRegex(const QString &glob)
{
    QString rest = glob;
    QString schemeRe = QStringLiteral("https?");
    const int schemeEnd = rest.indexOf(QStringLiteral("://"));
    if (schemeEnd >= 0) {
        const QString scheme = rest.left(schemeEnd);
        if (scheme != QLatin1String("*"))
            schemeRe = QRegularExpression::escape(scheme);
        rest = rest.mid(schemeEnd + 3);
    }

    // Host portion ends at the first '/' (path) or '?' (query-only
    // pattern). Ports/userinfo: the vendored lists use neither.
    const int slash = rest.indexOf(QLatin1Char('/'));
    const int question = rest.indexOf(QLatin1Char('?'));
    int hostEnd;
    if (slash >= 0 && question >= 0)
        hostEnd = qMin(slash, question);
    else
        hostEnd = qMax(slash, question);
    QString host = hostEnd >= 0 ? rest.left(hostEnd) : rest;
    const QString path = hostEnd >= 0 ? rest.mid(hostEnd) : QString();

    QString hostRe;
    if (host == QLatin1String("*")) {
        hostRe = QStringLiteral("[^/?:]*");
    } else {
        if (host.startsWith(QLatin1String("*."))) {
            hostRe = QStringLiteral("(?:[^/?:.]+\\.)*");
            host = host.mid(2);
        }
        hostRe += globEscape(host);
    }

    return QRegularExpression(QStringLiteral("\\A(?i:%1://%2)%3\\z")
                                  .arg(schemeRe, hostRe, globEscape(path)));
}

bool matchesAny(const QList<QRegularExpression> &set, const QString &spec)
{
    for (const QRegularExpression &re : set) {
        if (re.match(spec).hasMatch())
            return true;
    }
    return false;
}

// The shape URLPattern-style globs are written against:
// scheme://host/path?query (no userinfo/port, no fragment, host in ACE).
QString matchCandidate(const QUrl &url)
{
    QString candidate =
        url.scheme() + QStringLiteral("://") + QUrl::toAce(url.host());
    candidate += url.path(QUrl::FullyEncoded);
    const QString query = url.query(QUrl::FullyEncoded);
    if (!query.isEmpty())
        candidate += QLatin1Char('?') + query;
    return candidate;
}

QString specSansFragment(const QString &spec)
{
    const int hash = spec.indexOf(QLatin1Char('#'));
    return hash < 0 ? spec : spec.left(hash);
}

// UnescapeURLComponent(SPACES | PATH_SEPARATORS |
// URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS | REPLACE_PLUS_WITH_SPACE):
// full percent-decode, then literal '+' → space.
QString percentDecodePlus(const QString &in)
{
    return QString::fromUtf8(
        QByteArray::fromPercentEncoding(in.toUtf8()).replace('+', ' '));
}

// Brave's NaivelyExtractHostnameFromUrl: strip http(s):// prefix, split
// on ':', '/', '?', take the first non-empty piece. Case preserved on
// purpose — the compare against the canonical parsed host is meant to
// fail on case/parser confusion.
QString naiveHost(const QString &spec)
{
    QString s = spec;
    if (s.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
        s = s.mid(8);
    else if (s.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive))
        s = s.mid(7);
    static const QRegularExpression separators(QStringLiteral("[:/?]"));
    const QStringList parts = s.split(separators, Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.first();
}

// ------------------------------------------------- strip (stages 2 + 3)

// Brave's shared byte-preserving strip: split the raw query on '&',
// tokens with no '=', an empty name, or an empty value pass through
// untouched; names match exactly, case-sensitively, percent-encoded.
// Rejoin survivors with '&'; everything stripped → drop the '?'.
bool stripFromSpec(QString &spec, const QSet<QString> &blocklist,
                   CleanResult &result)
{
    const int hashPos = spec.indexOf(QLatin1Char('#'));
    const int searchEnd = hashPos < 0 ? spec.size() : hashPos;
    const int qPos = spec.indexOf(QLatin1Char('?'));
    if (qPos < 0 || qPos >= searchEnd)
        return false;

    const QString query = spec.mid(qPos + 1, searchEnd - qPos - 1);
    const QStringList tokens =
        query.split(QLatin1Char('&'), Qt::KeepEmptyParts);
    QStringList survivors;
    bool strippedAny = false;
    for (const QString &token : tokens) {
        const int eq = token.indexOf(QLatin1Char('='));
        const QString name = eq > 0 ? token.left(eq) : QString();
        const QString value = eq > 0 ? token.mid(eq + 1) : QString();
        // no '=' / empty name / empty value pass through untouched
        const bool strip =
            !name.isEmpty() && !value.isEmpty() && blocklist.contains(name);
        if (strip) {
            strippedAny = true;
            if (!result.strippedParams.contains(name))
                result.strippedParams.append(name);
            ++result.strippedCount;
        } else {
            survivors.append(token);
        }
    }
    if (!strippedAny)
        return false;

    QString rebuilt = survivors.isEmpty()
        ? spec.left(qPos) // drop the '?' entirely
        : spec.left(qPos + 1) + survivors.join(QLatin1Char('&'));
    spec = rebuilt + spec.mid(searchEnd); // re-attach fragment
    return true;
}

// Hardcoded conditional trackers (Brave components/query_filter): strip
// only when the full URL spec does NOT match the keep-regex (RE2 partial
// match).
struct ConditionalTracker {
    const char *param;
    const char *keepRegex;
};
const ConditionalTracker kConditionalTrackers[] = {
    {"mkt_tok", "([uU]nsubscribe|emailWebview)"},
    {"h_sid", "/email/"},
    {"h_slt", "/email/"},
    {"ck_subscriber_id", "/unsubscribe"},
};

} // namespace

// ------------------------------------------------------------- loading

bool UrlCleaner::loadLists(const QString &dir)
{
    bool ok = true;
    ok &= m_tld.load(dir + QStringLiteral("/public_suffix_list.dat"));
    ok &= loadDebounce(dir + QStringLiteral("/debounce.json"));
    ok &= loadStripRules(dir + QStringLiteral("/query-filter.json"),
                         m_queryFilterRules);
    ok &= loadStripRules(dir + QStringLiteral("/clean-urls.json"),
                         m_cleanUrlsRules);
    if (!m_tld.isLoaded() && !m_debounceRules.isEmpty()) {
        // Debounce failsafes are computed from the PSL; without it the
        // stage cannot run safely and no-ops instead.
        qWarning("urlclean: PSL failed to load — debounce disabled");
        m_debounceRules.clear();
    }
    return ok;
}

bool UrlCleaner::loadStripRules(const QString &path, QList<StripRule> &out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return false;

    for (const QJsonValue &entry : doc.array()) {
        const QJsonObject obj = entry.toObject();
        StripRule rule;
        for (const QJsonValue &v : obj[QStringLiteral("include")].toArray())
            rule.include.append(globToRegex(v.toString()));
        for (const QJsonValue &v : obj[QStringLiteral("exclude")].toArray())
            rule.exclude.append(globToRegex(v.toString()));
        for (const QJsonValue &v : obj[QStringLiteral("params")].toArray())
            rule.params.insert(v.toString());
        if (rule.include.isEmpty() || rule.params.isEmpty()) {
            qWarning("urlclean: skipping invalid strip rule in %s",
                     qPrintable(path));
            continue;
        }
        out.append(rule);
    }
    return true;
}

bool UrlCleaner::loadDebounce(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return false;

    for (const QJsonValue &entry : doc.array()) {
        const QJsonObject obj = entry.toObject();
        DebounceRule rule;

        const QString action = obj[QStringLiteral("action")].toString();
        if (action == QLatin1String("redirect"))
            rule.action = DebounceRule::Redirect;
        else if (action == QLatin1String("base64,redirect"))
            rule.action = DebounceRule::Base64Redirect;
        else if (action == QLatin1String("regex-path"))
            rule.action = DebounceRule::RegexPath;
        else if (action == QLatin1String("regex-path-template"))
            rule.action = DebounceRule::RegexPathTemplate;
        else {
            qWarning("urlclean: skipping rule with unknown action %s",
                     qPrintable(action));
            continue;
        }

        const QString prepend =
            obj[QStringLiteral("prepend_scheme")].toString();
        if (prepend == QLatin1String("http")
            || prepend == QLatin1String("https"))
            rule.prependScheme = prepend;
        else if (!prepend.isEmpty()) {
            qWarning("urlclean: skipping rule with unknown scheme %s",
                     qPrintable(prepend));
            continue;
        }

        rule.param = obj[QStringLiteral("param")].toString();
        rule.redirectTemplate =
            obj[QStringLiteral("redirect_url_template")].toString();
        // "pref"-gated rules are applied unconditionally (Luch has no
        // pref system; Brave defaults them on).

        if (rule.action == DebounceRule::RegexPath
            || rule.action == DebounceRule::RegexPathTemplate) {
            rule.regex = QRegularExpression(rule.param);
            // Verify at load, log + skip on failure — mirrors Brave's
            // fail-closed-per-rule behavior.
            if (!rule.regex.isValid()
                || rule.regex.captureCount() < 1) {
                qWarning("urlclean: skipping rule with bad regex %s",
                         qPrintable(rule.param));
                continue;
            }
        }

        for (const QJsonValue &v : obj[QStringLiteral("include")].toArray())
            rule.include.append(globToRegex(v.toString()));
        for (const QJsonValue &v : obj[QStringLiteral("exclude")].toArray())
            rule.exclude.append(globToRegex(v.toString()));
        if (rule.include.isEmpty()) {
            qWarning("urlclean: skipping debounce rule with no includes");
            continue;
        }
        m_debounceRules.append(rule);
    }
    return true;
}

// ------------------------------------------------------------ debounce

QString UrlCleaner::debounce(const QUrl &url, bool &changed,
                             bool &shortener) const
{
    changed = false;
    shortener = false;
    const QString candidate = matchCandidate(url);

    // First rule that produces a valid result wins; rules whose
    // extraction or failsafes fail are skipped past (Brave's service
    // loop), one hop only.
    for (const DebounceRule &rule : m_debounceRules) {
        if (matchesAny(rule.exclude, candidate))
            continue;
        if (!matchesAny(rule.include, candidate))
            continue;
        // The host matches a wrapper pattern — flag it even when this
        // rule's extraction/failsafes fail below (the online fallback
        // trigger is data-driven from this flag).
        shortener = true;

        QString value;
        if (rule.action == DebounceRule::RegexPath
            || rule.action == DebounceRule::RegexPathTemplate) {
            // Regex applies to ONLY the path of the original URL.
            const QString path = url.path(QUrl::FullyEncoded);
            const QRegularExpressionMatch match = rule.regex.match(path);
            if (!match.hasMatch())
                continue;
            const QStringList captures =
                match.capturedTexts().mid(1); // skip the full match

            if (rule.action == DebounceRule::RegexPathTemplate) {
                if (captures.size() > 9)
                    continue;
                // Placeholders ($1..$9) and capture groups must match
                // exactly: every group referenced, every placeholder real.
                QSet<int> placeholders;
                for (int j = 0; j + 1 < rule.redirectTemplate.size(); ++j) {
                    if (rule.redirectTemplate[j] == QLatin1Char('$')
                        && rule.redirectTemplate[j + 1] >= QLatin1Char('1')
                        && rule.redirectTemplate[j + 1] <= QLatin1Char('9'))
                        placeholders.insert(
                            rule.redirectTemplate[j + 1].digitValue());
                }
                bool exact = true;
                for (int i = 1; i <= captures.size(); ++i)
                    exact &= placeholders.remove(i);
                if (!exact || !placeholders.isEmpty())
                    continue;

                value = rule.redirectTemplate;
                for (int i = 0; i < captures.size(); ++i)
                    value.replace(QStringLiteral("$%1").arg(i + 1),
                                  captures[i]);
            } else {
                // Concatenate non-empty captures.
                for (const QString &group : captures) {
                    if (!group.isEmpty())
                        value += group;
                }
            }
            if (!value.isEmpty())
                value = percentDecodePlus(value);
        } else {
            const QUrlQuery query(url);
            value = query.queryItemValue(rule.param, QUrl::FullyDecoded);
            if (value.isNull())
                continue;
            if (rule.action == DebounceRule::Base64Redirect) {
                const QByteArray::FromBase64Result decoded =
                    QByteArray::fromBase64Encoding(
                        value.toUtf8(),
                        QByteArray::Base64UrlEncoding
                            | QByteArray::OmitTrailingEquals
                            | QByteArray::AbortOnBase64DecodingErrors);
                if (decoded.decodingStatus
                    != QByteArray::Base64DecodingStatus::Ok)
                    continue; // decode failure = rule miss
                value = QString::fromUtf8(decoded.decoded);
            }
        }

        // prepend_scheme: if the extracted value is already a valid URL
        // (has a scheme), the rule is erroneous — reject; else prepend
        // and re-validate.
        if (!rule.prependScheme.isEmpty()) {
            const QUrl test(value);
            if (test.isValid() && !test.scheme().isEmpty())
                continue;
            value = rule.prependScheme + QStringLiteral("://") + value;
        }

        const QUrl newUrl(value);
        // Failsafe: the result must be a valid http(s) URL.
        if (!newUrl.isValid()
            || (newUrl.scheme() != QLatin1String("http")
                && newUrl.scheme() != QLatin1String("https")))
            continue;
        // Failsafe: never redirect to the same site (host equality or
        // shared eTLD+1, ICANN-only).
        bool same = newUrl.host() == url.host();
        if (!same) {
            const QString domain = m_tld.eTLDPlusOne(newUrl.host());
            same = !domain.isEmpty()
                && domain == m_tld.eTLDPlusOne(url.host());
        }
        if (same)
            continue;
        // Failsafe: naive-parse hostname must equal the parsed host
        // (anti parser-confusion; compare against the canonical
        // punycode/lowercase, dot-stripped form).
        QString parsed = QUrl::toAce(newUrl.host());
        while (parsed.endsWith(QLatin1Char('.')))
            parsed.chop(1);
        if (naiveHost(value) != parsed)
            continue;
        // Failsafe: destination must have a non-empty eTLD+1.
        if (m_tld.eTLDPlusOne(newUrl.host()).isEmpty())
            continue;

        if (value != url.toString(QUrl::FullyEncoded)) {
            changed = true;
            return value;
        }
        // Rule produced the original URL: keep scanning (Brave).
    }
    return QString();
}

// --------------------------------------------------------- strip stages

void UrlCleaner::stripStage(const QList<StripRule> &rules, bool unionMode,
                            const QUrl &url, QString &spec,
                            CleanResult &result) const
{
    if (unionMode) {
        QSet<QString> blocklist;
        const QString target = specSansFragment(spec);
        for (const StripRule &rule : rules) {
            if (matchesAny(rule.exclude, target)
                || !matchesAny(rule.include, target))
                continue;
            blocklist.unite(rule.params);
        }
        for (const ConditionalTracker &tracker : kConditionalTrackers) {
            const QRegularExpression keep(
                QString::fromLatin1(tracker.keepRegex));
            if (!keep.match(target).hasMatch())
                blocklist.insert(QString::fromLatin1(tracker.param));
        }
        if (!blocklist.isEmpty() && stripFromSpec(spec, blocklist, result))
            result.changed = true;
        return;
    }

    // clean-urls: applied per matcher sequentially, each seeing the
    // progressively stripped URL.
    for (const StripRule &rule : rules) {
        const QString target = specSansFragment(spec);
        if (matchesAny(rule.exclude, target)
            || !matchesAny(rule.include, target))
            continue;
        if (stripFromSpec(spec, rule.params, result))
            result.changed = true;
    }
}

// --------------------------------------------------------- public stages

DebounceResult UrlCleaner::unwrap(const QUrl &url) const
{
    DebounceResult result;
    bool changed = false;
    const QString unwrapped = debounce(url, changed, result.shortener);
    if (changed) {
        result.url = unwrapped;
        result.changed = true;
        result.debouncedFrom = url.host();
    }
    return result;
}

CleanResult UrlCleaner::strip(const QUrl &url) const
{
    CleanResult result;
    QString spec = url.toString(QUrl::FullyEncoded);
    result.url = spec;

    stripStage(m_queryFilterRules, /*unionMode=*/true, url, spec,
               result);
    stripStage(m_cleanUrlsRules, /*unionMode=*/false, url, spec,
               result);

    result.url = spec;
    return result;
}

} // namespace Luch
