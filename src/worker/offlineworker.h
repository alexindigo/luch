#pragma once

#include <QThread>

namespace Luch {

// QThread hosting every inet:false plugin stage. On start it installs
// the seccomp network sandbox: socket() is allowed for AF_UNIX (D-Bus,
// local IPC) and fails with EAFNOSUPPORT for AF_INET/AF_INET6 — an
// offline plugin provably cannot make an outside request at the syscall
// level. The filter is per-thread; sibling threads are unaffected.
// Plugin stages are dispatched onto this thread sequentially via its
// runner (blocking queued invocations — µs-fast stages make
// serialization free).
class OfflineWorker : public QThread
{
    Q_OBJECT

public:
    explicit OfflineWorker(QObject *parent = nullptr);
    ~OfflineWorker() override;

protected:
    void run() override;
};

} // namespace Luch
