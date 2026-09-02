#pragma once

#include <QObject>
#include <QtPlugin>

#include "luchaugmenter.h"
#include "urlcleaner.h"

// Clean stage plugin: Brave-parity tracker stripping (query-filter +
// clean-urls stages). Idempotent — converges in one pass.
class BraveCleanUrlPlugin : public QObject, public LuchTargetAugmenter
{
    Q_OBJECT
    // IID tag only — manifest metadata lives in the sidecar
    // bravecleanurlplugin.json, installed as brave-clean-url.json next
    // to the .so.
    Q_PLUGIN_METADATA(IID LuchTargetAugmenter_iid)
    Q_INTERFACES(LuchTargetAugmenter)

public:
    void init(const QVariantMap &userConfig) override;
    QVariantMap augment(const QVariantMap &chainState,
                        const QVariantMap &priorSlices) const override;

private:
    Luch::UrlCleaner m_cleaner;
};
