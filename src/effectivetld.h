#pragma once

#include <QSet>
#include <QString>

namespace Luch {

// Minimal Public Suffix List matcher (ICANN section only), used for the
// debounce failsafes. Equivalent of Chromium's
// registry_controlled_domains::GetDomainAndRegistry(host,
// EXCLUDE_PRIVATE_REGISTRIES).
class EffectiveTld
{
public:
    // Parses the ICANN section of public_suffix_list.dat (between the
    // BEGIN/END ICANN DOMAINS markers). Returns false if the file is
    // missing or the markers are absent.
    bool load(const QString &pslPath);

    // The registrable domain ("eTLD+1") for host, or "" when the host has
    // none (single label, host itself is a public suffix, unknown/empty).
    bool isLoaded() const { return m_loaded; }
    QString eTLDPlusOne(const QString &host) const;

private:
    QSet<QString> m_exact;     // "example.com"
    QSet<QString> m_wildcard;  // "ck" for a "*.ck" rule
    QSet<QString> m_exception; // "www.ck" for a "!www.ck" rule
    bool m_loaded = false;
};

} // namespace Luch
