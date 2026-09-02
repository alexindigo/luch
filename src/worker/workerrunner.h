#pragma once

#include <QObject>
#include <QVariantMap>

namespace Luch {

// Per-thread dispatch target for plugin stages. Created inside the
// worker's run() so its affinity is the worker thread; invokeMethod
// lambdas queued on it execute in that thread. Carries the inet
// completion signal from the worker thread back to the pipeline.
class WorkerRunner : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void inetDone(quint64 token, const QVariantMap &slice);
};

} // namespace Luch
