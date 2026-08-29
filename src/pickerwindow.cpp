#include "pickerwindow.h"

#include <LayerShellQt/Window>
#include <QEvent>
#include <QQuickWindow>

namespace Luch {

PickerWindow::PickerWindow(QQuickWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    Q_ASSERT(window);
    configureLayerShell();
    window->installEventFilter(this);
    connect(window, &QQuickWindow::closing, this, [this] {
        Q_EMIT dismissed(1);
    });
}

void PickerWindow::configureLayerShell()
{
    auto *layer = LayerShellQt::Window::get(m_window);
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setAnchors({});
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityExclusive);
    layer->setScope(QStringLiteral("luch-picker"));
}

bool PickerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::WindowActivate:
            m_wasActive = true;
            break;
        case QEvent::WindowDeactivate:
            if (m_wasActive)
                m_window->close();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace Luch
