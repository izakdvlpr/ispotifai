#include "http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <ispotifai/version.hpp>

#include "log.hpp"
#include "network.hpp"

namespace ispotifai {

namespace {

constexpr int kBacklog    = 4;
constexpr size_t kReqSize = 1024;

// A client that connects and vanishes before accept() returns gives
// ECONNABORTED, which is normal and must not take the server down. A listening
// socket that the system has torn down gives it forever, which must.
constexpr int kMaxAcceptErrors = 10;
constexpr u64 kAcceptRetryNs   = 100ull * 1000000ull; // 100ms

const char *const kPage =
    "<!doctype html>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>iSpotifai</title>"
    "<style>"
    "body{background:#121212;color:#fff;font:16px system-ui,sans-serif;"
    "display:grid;place-items:center;height:100vh;margin:0}"
    "h1{color:#1DB954;margin:0 0 .5rem}"
    "p{color:#9E9E9E;margin:0}"
    "</style>"
    "<div><h1>iSpotifai</h1><p>Sysmodule is running.</p></div>";

// The loop is single threaded, so a client that opens a connection and says
// nothing must not be able to wedge the server.
void setTimeouts(int fd) {
    struct timeval tv = {};
    tv.tv_sec         = 5;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool sendAll(int fd, const char *data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            logf("send failed after %zu/%zu bytes, errno=%d", sent, len, errno);
            return false;
        }

        sent += static_cast<size_t>(n);
    }

    return true;
}

bool sendResponse(int fd, const char *status, const char *contentType, const char *body) {
    size_t bodyLen = strlen(body);

    char header[256];
    int headerLen = snprintf(header, sizeof(header),
                             "HTTP/1.1 %s\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %zu\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             status, contentType, bodyLen);

    if (headerLen <= 0 || static_cast<size_t>(headerLen) >= sizeof(header)) {
        logf("header snprintf failed (%d)", headerLen);
        return false;
    }

    if (!sendAll(fd, header, static_cast<size_t>(headerLen)))
        return false;

    return sendAll(fd, body, bodyLen);
}

// Reads until the end of the request headers. Stopping early would leave bytes
// in the receive buffer, and closing a socket with unread data sends an RST
// instead of a FIN -- the client then sees "connection reset by peer" even
// though the response went out fine.
ssize_t readRequest(int fd, char *buf, size_t size) {
    size_t total = 0;

    while (total < size - 1) {
        ssize_t n = recv(fd, buf + total, size - 1 - total, 0);
        if (n < 0) {
            logf("recv failed, errno=%d", errno);
            return -1;
        }

        if (n == 0)
            break; // client closed its side

        total += static_cast<size_t>(n);
        buf[total] = '\0';

        if (strstr(buf, "\r\n\r\n") || strstr(buf, "\n\n"))
            break;
    }

    buf[total] = '\0';
    return static_cast<ssize_t>(total);
}

// Everything the client still has in flight has to be consumed before close,
// same RST reason as above.
void drainAndClose(int fd) {
    shutdown(fd, SHUT_WR);

    char sink[256];
    for (int i = 0; i < 8; i++) {
        ssize_t n = recv(fd, sink, sizeof(sink), 0);
        if (n <= 0)
            break;
    }

    close(fd);
}

// These mean the listening socket itself is gone -- retrying accept() on it can
// only fail again.
bool isFatalAcceptError(int err) {
    return err == EBADF || err == ENOTSOCK || err == EINVAL || err == ENETDOWN ||
           err == ENETUNREACH;
}

void closeListener(int fd) {
    networkUnregisterSocket(fd);
    close(fd);
}

void handleClient(int fd, const char *peer) {
    char request[kReqSize];

    ssize_t got = readRequest(fd, request, sizeof(request));
    if (got <= 0) {
        logf("%s: no request (%zd)", peer, got);
        drainAndClose(fd);
        return;
    }

    // Only the request line matters here: "GET /path HTTP/1.1"
    bool root = strncmp(request, "GET / ", 6) == 0 || strncmp(request, "GET /?", 6) == 0;

    bool ok = root
                  ? sendResponse(fd, "200 OK", "text/html; charset=utf-8", kPage)
                  : sendResponse(fd, "404 Not Found", "text/plain; charset=utf-8", "not found\n");

    // First token of the request line, for the log.
    char line[64];
    size_t n = 0;
    while (n < sizeof(line) - 1 && request[n] && request[n] != '\r' && request[n] != '\n') {
        line[n] = request[n];
        n++;
    }
    line[n] = '\0';

    logf("%s: \"%s\" -> %s (%s)", peer, line, root ? "200" : "404", ok ? "sent" : "failed");

    drainAndClose(fd);
}

} // namespace

ServerExit runHttpServer(u16 port) {
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        logf("socket() failed, errno=%d", errno);
        return ServerExit::SetupFailed;
    }

    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        logf("bind(%u) failed, errno=%d", port, errno);
        close(listenFd);
        return ServerExit::SetupFailed;
    }

    if (listen(listenFd, kBacklog) < 0) {
        logf("listen() failed, errno=%d", errno);
        close(listenFd);
        return ServerExit::SetupFailed;
    }

    // Ties the socket to the nifm request, so the system keeps it across sleep.
    networkRegisterSocket(listenFd);

    logf("listening on port %u", port);

    int errorRun = 0; // consecutive accept() failures

    for (;;) {
        // libnx forwards these straight to bsd:u as the IPC output buffer, so
        // they must be real storage -- passing NULL here is not safe.
        struct sockaddr_in client = {};
        socklen_t clientLen       = sizeof(client);

        int clientFd = accept(listenFd, reinterpret_cast<struct sockaddr *>(&client), &clientLen);
        if (clientFd < 0) {
            int err = errno;
            errorRun++;

            if (isFatalAcceptError(err) || errorRun >= kMaxAcceptErrors || !networkIsUp()) {
                logf("accept() gave up after %d error(s), errno=%d", errorRun, err);
                closeListener(listenFd);
                return ServerExit::NetworkLost;
            }

            logf("accept() failed, errno=%d", err);
            svcSleepThread(kAcceptRetryNs); // don't spin
            continue;
        }

        errorRun = 0;

        char peer[16] = "?"; // "255.255.255.255" + NUL
        inet_ntop(AF_INET, &client.sin_addr, peer, sizeof(peer));

        setTimeouts(clientFd);
        handleClient(clientFd, peer);
    }
}

} // namespace ispotifai
