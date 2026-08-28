#include "xdgactivation.h"

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <qwayland-xdg-activation-v1.h>

#include <wayland-client.h>

#include <functional>

class XdgActivationTokenRequester::TokenObject
    : public QtWayland::xdg_activation_token_v1
{
public:
    using QtWayland::xdg_activation_token_v1::xdg_activation_token_v1;

    std::function<void(const QString &)> onDone;
    quint64 generation = 0;

protected:
    void xdg_activation_token_v1_done(const QString &token) override
    {
        if (onDone)
            onDone(token);
        delete this;
    }
};

struct XdgActivationTokenRequester::Private
{
    QtWayland::xdg_activation_v1 activation;
    wl_registry *registry = nullptr;
    TokenObject *pendingToken = nullptr;
    quint64 generation = 0;
};

namespace {

} // namespace

void XdgActivationTokenRequester::registryHandleGlobal(
    void *data, wl_registry *registry, uint32_t name, const char *interface,
    uint32_t version)
{
    auto *d = static_cast<Private *>(data);
    if (qstrcmp(interface, "xdg_activation_v1") == 0
        && !d->activation.isInitialized())
        d->activation.init(registry, name, qMin(version, 1u));
}

void XdgActivationTokenRequester::registryHandleRemove(void *data,
                                                       wl_registry *registry,
                                                       uint32_t name)
{
    Q_UNUSED(data);
    Q_UNUSED(registry);
    Q_UNUSED(name);
}

XdgActivationTokenRequester::XdgActivationTokenRequester(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    auto *waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApp || !waylandApp->display())
        return;

    d->registry = wl_display_get_registry(waylandApp->display());
    if (d->registry) {
        static const wl_registry_listener listener = {
            &XdgActivationTokenRequester::registryHandleGlobal,
            &XdgActivationTokenRequester::registryHandleRemove,
        };
        wl_registry_add_listener(d->registry, &listener, d);
    }
}

XdgActivationTokenRequester::~XdgActivationTokenRequester()
{
    delete d->pendingToken;
    delete d;
}

void XdgActivationTokenRequester::requestToken(QWindow *window,
                                               const QString &appId)
{
    if (!d->activation.isInitialized()) {
        Q_EMIT failed();
        return;
    }

    auto *waylandApp =
        qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApp) {
        Q_EMIT failed();
        return;
    }

    wl_seat *seat = waylandApp->lastInputSeat()
                        ? waylandApp->lastInputSeat()
                        : waylandApp->seat();
    const uint serial = waylandApp->lastInputSerial();
    if (!seat) {
        Q_EMIT failed();
        return;
    }

    auto *nativeInterface = QGuiApplication::platformNativeInterface();
    auto *surface = nativeInterface
        ? static_cast<wl_surface *>(nativeInterface->nativeResourceForWindow(
              QByteArrayLiteral("surface"), window))
        : nullptr;
    if (!surface) {
        Q_EMIT failed();
        return;
    }

    const quint64 generation = ++d->generation;
    if (d->pendingToken)
        d->pendingToken->onDone = nullptr;

    auto *token = new TokenObject();
    token->init(d->activation.get_activation_token());
    token->set_serial(serial, seat);
    token->set_surface(surface);
    if (!appId.isEmpty())
        token->set_app_id(appId);
    token->commit();
    token->onDone = [this, token, generation](const QString &tokenString) {
        if (generation != d->generation) {
            delete token;
            return;
        }
        d->pendingToken = nullptr;
        Q_EMIT tokenReady(tokenString);
    };
    d->pendingToken = token;

    wl_display_flush(waylandApp->display());
}
