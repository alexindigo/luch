#pragma once

#include <QObject>

class QQuickWindow;

namespace Luch {

class PickerWindow : public QObject
{
    Q_OBJECT

public:
    explicit PickerWindow(QQuickWindow *window, QObject *parent = nullptr);

Q_SIGNALS:
    void dismissed(int exitCode);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void configureLayerShell();

    QQuickWindow *m_window = nullptr;
    bool m_wasActive = false;
};

} // namespace Luch
