#pragma once

#include <QThread>

namespace Luch {

// QThread with an event loop hosting every inet:true plugin stage (and
// the QNetworkAccessManager for future online plugins). Unfiltered —
// the network capability lives here and nowhere else. Stages are
// dispatched queued onto its runner: a slow plugin must never block
// the UI thread; its slice lands via patchSlice when it completes.
class InetWorker : public QThread
{
    Q_OBJECT

public:
    explicit InetWorker(QObject *parent = nullptr);
    ~InetWorker() override;
};

} // namespace Luch
