#include "inetworker.h"

namespace Luch {

InetWorker::InetWorker(QObject *parent)
    : QThread(parent)
{
}

InetWorker::~InetWorker()
{
    quit();
    wait();
}

} // namespace Luch

#include "inetworker.moc"
