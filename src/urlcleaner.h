#pragma once

#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "effectivetld.h"

namespace Luch {

struct CleanResult
{
    QString url;            // cleaned (== input spec when unchanged)
    bool changed = false;
    int strippedCount = 0;
    QStringList strippedParams; // encounter order, deduped
    bool debounced = false;
    QString debouncedFrom;  // wrapper host when debounced
};

// Brave-parity URL cleaner: debounce → query-filter → clean-urls, driven
// by Brave's vendored data lists. QtCore-only. Replicates Brave's behavior
// (components/debounce, components/query_filter, components/url_sanitizer)
// including failsafes; see the plan for the full digest.
class UrlCleaner
{
public:
    // Reads debounce.json, query-filter.json, clean-urls.json and
    // public_suffix_list.dat from dir. Per-file failure → that stage
    // no-ops. Returns true if all four loaded.
    bool loadLists(const QString &dir);

    CleanResult clean(const QUrl &url) const;

private:
    struct DebounceRule {
        QList<QRegularExpression> include;
        QList<QRegularExpression> exclude;
        enum Action { Redirect, Base64Redirect, RegexPath,
                      RegexPathTemplate } action = Redirect;
        QString param;            // query param name or regex pattern
        QString prependScheme;    // "" | "http" | "https"
        QString redirectTemplate; // regex-path-template only
        QRegularExpression regex; // compiled param for regex actions
    };
    struct StripRule {
        QList<QRegularExpression> include;
        QList<QRegularExpression> exclude;
        QSet<QString> params;     // raw, still percent-encoded names
    };

    bool loadDebounce(const QString &path);
    bool loadStripRules(const QString &path, QList<StripRule> &out);

    QString debounce(const QUrl &url, bool &changed) const;
    void stripStage(const QList<StripRule> &rules, bool unionMode,
                    const QUrl &url, QString &spec,
                    CleanResult &result) const;

    EffectiveTld m_tld;
    QList<DebounceRule> m_debounceRules;
    QList<StripRule> m_queryFilterRules;
    QList<StripRule> m_cleanUrlsRules;
};

} // namespace Luch
