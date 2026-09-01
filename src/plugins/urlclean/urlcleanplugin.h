#pragma once

#include <QObject>
#include <QtPlugin>

#include "luchaugmenter.h"
#include "urlcleaner.h"

// First self-hosting plugin: Brave-parity URL cleaning (debounce →
// query-filter → clean-urls) driven by the vendored Brave lists.
class UrlCleanPlugin : public QObject, public LuchTargetAugmenter
{
    Q_OBJECT
    // IID tag only — manifest metadata lives in the sidecar
    // urlcleanplugin.json, installed as urlclean.json next to the .so.
    Q_PLUGIN_METADATA(IID LuchTargetAugmenter_iid)
    Q_INTERFACES(LuchTargetAugmenter)

public:
    void init(const QVariantMap &userConfig) override;
    QVariantMap augment(const QVariantMap &target,
                        const QVariantMap &priorSlices) const override;

private:
    Luch::UrlCleaner m_cleaner;
};
