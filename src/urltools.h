#pragma once

#include <QObject>
#include <QString>

#include "effectivetld.h"

namespace Luch {

// QML-facing URL utilities backed by the engine. Currently: eTLD+1 for
// the dissection panel's domain row.
class UrlTools : public QObject
{
    Q_OBJECT

public:
    explicit UrlTools(QObject *parent = nullptr);

    // The registrable domain ("eTLD+1") for host, or "" when the host
    // has none (single label, public suffix, unknown/empty).
    Q_INVOKABLE QString eTLDPlusOne(const QString &host) const;

private:
    EffectiveTld m_tld;
};

} // namespace Luch
