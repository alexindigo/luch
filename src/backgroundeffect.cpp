#include "backgroundeffect.h"

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QQuickWindow>
#include <QVariantMap>
#include <qwayland-ext-background-effect-v1.h>

#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include <functional>

namespace {

class EffectManager
    : public QtWayland::ext_background_effect_manager_v1
{
public:
    bool blurCapable = false;
    std::function<void()> onCapabilities;

protected:
    void ext_background_effect_manager_v1_capabilities(
        uint32_t flags) override
    {
        blurCapable = flags & capability_blur;
        if (blurCapable && onCapabilities)
            onCapabilities();
    }
};

} // namespace

struct BackgroundEffect::Private
{
    EffectManager manager;
    QtWayland::ext_background_effect_surface_v1 effect;
    wl_compositor *compositor = nullptr;
    bool effectCreated = false;
    bool dirty = false;
    QPointer<QWindow> window;
    QVariantList pendingRects;
};

BackgroundEffect::BackgroundEffect(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    auto *waylandApp =
        qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApp || !waylandApp->display())
        return; // not on Wayland — plain-alpha fallback

    d->manager.onCapabilities = [this] { applyPending(); };

    wl_registry *registry = wl_display_get_registry(waylandApp->display());
    if (!registry)
        return;
    static const wl_registry_listener listener = {
        [](void *data, wl_registry *registry, uint32_t name,
           const char *interface, uint32_t version) {
            auto *d = static_cast<Private *>(data);
            if (qstrcmp(interface, "ext_background_effect_manager_v1") == 0
                && !d->manager.isInitialized())
                d->manager.init(registry, name, qMin(version, 1u));
            else if (qstrcmp(interface, "wl_compositor") == 0
                     && !d->compositor)
                d->compositor = static_cast<wl_compositor *>(
                    wl_registry_bind(registry, name, &wl_compositor_interface,
                                     qMin(version, 1u)));
        },
        [](void *, wl_registry *, uint32_t) {},
    };
    wl_registry_add_listener(registry, &listener, d);
}

BackgroundEffect::~BackgroundEffect()
{
    if (d->effectCreated)
        d->effect.destroy();
    if (d->manager.isInitialized())
        d->manager.destroy();
    delete d;
}

void BackgroundEffect::setWindow(QWindow *window)
{
    d->window = window;
    if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
        // The wl_surface exists once the scene renders; retry until
        // then (Component.onCompleted runs before show()).
        connect(quick, &QQuickWindow::frameSwapped, this,
                &BackgroundEffect::applyPending, Qt::UniqueConnection);
    }
    applyPending();
}

void BackgroundEffect::setBlurRects(const QVariantList &rects)
{
    d->pendingRects = rects;
    d->dirty = true;
    applyPending();
}

void BackgroundEffect::applyPending()
{
    if (!d->dirty || !d->window)
        return;

    auto *waylandApp =
        qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApp || !waylandApp->display())
        return;

    auto *nativeInterface = QGuiApplication::platformNativeInterface();
    auto *surface = nativeInterface
        ? static_cast<wl_surface *>(nativeInterface->nativeResourceForWindow(
              QByteArrayLiteral("surface"), d->window))
        : nullptr;
    if (!surface)
        return; // not created yet — the frameSwapped hook retries

    if (!d->manager.isInitialized() || !d->manager.blurCapable
        || !d->compositor)
        return; // protocol/capability absent — plain-alpha fallback

    if (!d->effectCreated) {
        d->effect.init(d->manager.get_background_effect(surface));
        d->effectCreated = true;
    }

    wl_region *region = wl_compositor_create_region(d->compositor);
    if (!region)
        return;
    for (const QVariant &item : d->pendingRects) {
        const QVariantMap rect = item.toMap();
        wl_region_add(region, rect.value(QStringLiteral("x")).toInt(),
                      rect.value(QStringLiteral("y")).toInt(),
                      rect.value(QStringLiteral("width")).toInt(),
                      rect.value(QStringLiteral("height")).toInt());
    }
    // Copy semantics — the region can die immediately; the state lands
    // on the commit below.
    d->effect.set_blur_region(region);
    wl_region_destroy(region);
    wl_surface_commit(surface);
    wl_display_flush(waylandApp->display());

    d->dirty = false;
}
