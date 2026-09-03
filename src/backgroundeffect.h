#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantList>

class QWindow;

// Compositor backdrop blur for the picker surface via the
// ext-background-effect-v1 Wayland protocol. Binds the manager global
// from the registry; when the compositor doesn't speak the protocol
// (or reports no blur capability) everything degrades silently to the
// plain-alpha render — no error, no configuration.
class BackgroundEffect : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundEffect(QObject *parent = nullptr);
    ~BackgroundEffect() override;

    // The picker window whose wl_surface carries the effect. Regions
    // set before the surface exists apply on the first frame.
    void setWindow(QWindow *window);

    // rects: [{x, y, width, height}] in surface-local coordinates —
    // the translucent areas of the picker (main panel + any open
    // drawers). Double-buffered server-side; applied on commit.
    Q_INVOKABLE void setBlurRects(const QVariantList &rects);

private:
    void applyPending();

    struct Private;
    Private *d = nullptr;
};
