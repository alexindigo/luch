#pragma once

#include <QObject>
#include <QtPlugin>

#include "luchaugmenter.h"
#include "urlcleaner.h"

// Unwrap stage plugin: rule-based redirect-wrapper extraction from the
// URL itself (offline, Brave debounce list). Runs to fixpoint via the
// pipeline to unwrap nested wrappers.
class DebouncePlugin : public QObject, public LuchTargetAugmenter
{
    Q_OBJECT
    // IID tag only — manifest metadata lives in the sidecar
    // debounceplugin.json, installed as debounce.json next to the .so.
    Q_PLUGIN_METADATA(IID LuchTargetAugmenter_iid)
    Q_INTERFACES(LuchTargetAugmenter)

public:
    void init(const QVariantMap &userConfig) override;
    QVariantMap augment(const QVariantMap &chainState,
                        const QVariantMap &priorSlices) const override;

private:
    Luch::UrlCleaner m_cleaner;
};
