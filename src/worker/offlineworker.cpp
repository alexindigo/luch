#include "offlineworker.h"

#include <QScopeGuard>

#include <cerrno>
#include <sys/socket.h>

#include <seccomp.h>
#include <sys/prctl.h>

namespace Luch {

namespace {

// Binary, declared-capability network sandbox: allow AF_UNIX, block
// AF_INET/AF_INET6 (including DNS). Per-thread filter — no-new-privs
// is set for this thread only.
void installSeccompFilter()
{
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        qWarning("offlineworker: PR_SET_NO_NEW_PRIVS failed — sandbox "
                 "not installed");
        return;
    }
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (!ctx) {
        qWarning("offlineworker: seccomp_init failed — sandbox not "
                 "installed");
        return;
    }
    const auto release = qScopeGuard([ctx] { seccomp_release(ctx); });
    for (const int domain : {AF_INET, AF_INET6}) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EAFNOSUPPORT),
                             SCMP_SYS(socket), 1,
                             SCMP_A0(SCMP_CMP_EQ, domain)) != 0) {
            qWarning("offlineworker: seccomp_rule_add failed for "
                     "domain %d", domain);
            return;
        }
    }
    if (seccomp_load(ctx) != 0) {
        qWarning("offlineworker: seccomp_load failed — sandbox not "
                 "installed");
        return;
    }
}

} // namespace

OfflineWorker::OfflineWorker(QObject *parent)
    : QThread(parent)
{
}

OfflineWorker::~OfflineWorker()
{
    quit();
    wait();
}

void OfflineWorker::run()
{
    installSeccompFilter();
    exec();
}

} // namespace Luch

#include "offlineworker.moc"
