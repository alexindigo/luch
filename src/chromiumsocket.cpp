#include "chromiumsocket.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

namespace ChromiumSocket {

namespace {

constexpr const char *kStartToken = "START";
constexpr const char *kAckToken = "ACK";
constexpr const char *kShutdownToken = "SHUTDOWN";
constexpr const char kTokenDelimiter = '\0';
constexpr size_t kMaxAckBytes = 8; // strlen("SHUTDOWN")

// Source values from process_singleton_posix.cc: kRetryAttempts=20 over
// 20 seconds. Luch keeps the same cadence shape but a much smaller
// budget — the CLI floor bounds the total wait.
constexpr int kRetryAttempts = 3;
constexpr auto kSleepInterval = std::chrono::milliseconds(700);

std::string readLink(const std::string &path)
{
    std::string buffer(512, '\0');
    ssize_t len = readlink(path.c_str(), buffer.data(), buffer.size());
    if (len <= 0)
        return {};
    buffer.resize(static_cast<size_t>(len));
    return buffer;
}

bool cookieMatches(const std::string &dir, const std::string &cookie)
{
    return readLink(dir + "/SingletonCookie") == cookie;
}

bool lockFileProcessAlive(const std::string &dir)
{
    const std::string lock = readLink(dir + "/SingletonLock");
    const size_t dash = lock.rfind('-');
    if (dash == std::string::npos)
        return false;
    const long pid = std::strtol(lock.c_str() + dash + 1, nullptr, 10);
    return pid > 0 && kill(pid, 0) == 0;
}

// Mirrors ConnectSocket(): follow the SingletonSocket symlink, verify the
// cookie in the target directory before and after connecting. A
// non-symlink socket is an older Chromium; connect directly.
Result attemptDir(const std::string &dir, const std::string &cwd,
                  const std::vector<std::string> &argv, int ackTimeoutMs)
{
    const std::string socketLink = dir + "/SingletonSocket";
    const std::string cookieLink = dir + "/SingletonCookie";

    std::string target = readLink(socketLink);
    const bool haveSymlink = !target.empty();
    if (!haveSymlink && errno != EINVAL)
        return Result::NotRunning;

    std::string connectPath = haveSymlink ? target : socketLink;
    std::string cookie;
    if (haveSymlink) {
        cookie = readLink(cookieLink);
        if (cookie.empty())
            return Result::Failed;
        const size_t slash = target.rfind('/');
        if (slash == std::string::npos)
            return Result::Failed;
        const std::string targetDir = target.substr(0, slash);
        if (!cookieMatches(targetDir, cookie))
            return Result::Failed;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return Result::Failed;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (connectPath.size() >= sizeof(addr.sun_path)) {
        close(fd);
        return Result::Failed;
    }
    std::strncpy(addr.sun_path, connectPath.c_str(),
                 sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return Result::Failed;
    }

    if (haveSymlink) {
        const size_t slash = connectPath.rfind('/');
        const std::string targetDir =
            slash == std::string::npos ? std::string() : connectPath.substr(0, slash);
        if (!cookieMatches(targetDir, cookie)) {
            close(fd);
            return Result::Failed;
        }
    }

    std::string message(kStartToken);
    message.push_back(kTokenDelimiter);
    message.append(cwd);
    for (const std::string &arg : argv) {
        message.push_back(kTokenDelimiter);
        message.append(arg);
    }

    size_t sent = 0;
    while (sent < message.size()) {
        const ssize_t written =
            write(fd, message.data() + sent, message.size() - sent);
        if (written <= 0) {
            close(fd);
            return Result::Failed;
        }
        sent += static_cast<size_t>(written);
    }
    shutdown(fd, SHUT_WR);

    pollfd pfd{fd, POLLIN, 0};
    if (poll(&pfd, 1, ackTimeoutMs) <= 0) {
        close(fd);
        return Result::Failed;
    }

    char ack[kMaxAckBytes] = {};
    const ssize_t len = recv(fd, ack, kMaxAckBytes, 0);
    close(fd);
    if (len <= 0)
        return Result::Failed;

    const std::string reply(ack, static_cast<size_t>(len));
    if (reply == kAckToken)
        return Result::Acked;
    if (reply == kShutdownToken)
        return Result::NotRunning;
    return Result::Failed;
}

} // namespace

Result notifyRunningInstance(const std::vector<std::string> &userDataDirs,
                             const std::string &cwd,
                             const std::vector<std::string> &argv,
                             int ackTimeoutMs)
{
    for (const std::string &dir : userDataDirs) {
        struct stat st {};
        const std::string socketLink = dir + "/SingletonSocket";
        if (lstat(socketLink.c_str(), &st) != 0)
            continue;

        for (int attempt = 0; attempt <= kRetryAttempts; ++attempt) {
            const Result result = attemptDir(dir, cwd, argv, ackTimeoutMs);
            if (result == Result::Acked)
                return result;
            if (result == Result::NotRunning)
                break;
            if (!lockFileProcessAlive(dir))
                return Result::NotRunning;
            if (attempt < kRetryAttempts)
                std::this_thread::sleep_for(kSleepInterval);
        }
        return Result::Failed;
    }
    return Result::NotRunning;
}

} // namespace ChromiumSocket
