#pragma once

#include <QObject>
#include <QString>

class QWindow;
struct wl_registry;

class XdgActivationTokenRequester : public QObject
{
    Q_OBJECT

public:
    explicit XdgActivationTokenRequester(QObject *parent = nullptr);
    ~XdgActivationTokenRequester() override;

    void requestToken(QWindow *window, const QString &appId = QString());

Q_SIGNALS:
    void tokenReady(const QString &token);
    void failed();

private:
    class TokenObject;
    struct Private;
    static void registryHandleGlobal(void *data, wl_registry *registry,
                                     uint32_t name, const char *interface,
                                     uint32_t version);
    static void registryHandleRemove(void *data, wl_registry *registry,
                                     uint32_t name);
    Private *d = nullptr;
};
