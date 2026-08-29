#pragma once

#include <string>
#include <vector>

namespace ChromiumSocket {

enum class Result {
    Acked,       // running instance accepted the command
    NotRunning,  // no live SingletonSocket in any candidate dir
    Failed,      // socket present but handshake unexpected
};

// Notifies a running Chromium-family instance through its profile
// SingletonSocket, mirroring chrome/browser/process_singleton_posix.cc:
// connect to the symlinked socket (cookie-verified), send
// "START\0<cwd>\0<argv0>\0…" (NUL-framed, no trailing NUL), shutdown the
// write side, expect "ACK" (or "SHUTDOWN" = instance going away).
// The symlink is transiently stale on every Chromium startup, so the
// client retries with the source's sleep interval and verifies the
// lock-file pid is still alive before each retry, the way
// NotifyOtherProcessWithTimeout does. Any unexpected handshake yields
// Failed — callers fall back to spawning.
Result notifyRunningInstance(const std::vector<std::string> &userDataDirs,
                             const std::string &cwd,
                             const std::vector<std::string> &argv,
                             int ackTimeoutMs);

} // namespace ChromiumSocket
