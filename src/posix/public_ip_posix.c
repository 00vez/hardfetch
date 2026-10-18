#define _GNU_SOURCE
#include "../public_ip.h"
#include "../debug.h"
#include "../output.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static const char* const kHosts[] = {
    "api.ipify.org", "ifconfig.me", "icanhazip.com"
};
static const char* const kPaths[] = { "/", "/ip", "/" };

#define CRLF "\r\n"

/* Every step is bounded. A blocking connect on a filtered port hangs for the
   kernel default, which is over two minutes, and getaddrinfo has no timeout
   at all, so the resolution runs on its own thread. */
#define DNS_TIMEOUT_MS     1500
#define CONNECT_TIMEOUT_MS 1500
#define READ_TIMEOUT_MS    2000

typedef struct {
    char  buf[64];
    int   done;
} lookup;

/* The response needs its own buffer: the headers alone are longer than the
   address, so reading into the result would never reach the body. */
static int fetch_one(const char* host, const char* path, char* out, size_t outLen)
{
    static char resp[2048];
    struct addrinfo hints, *res = NULL, *rp;
    char port[16];
    int fd = -1;

    out[0] = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port, sizeof(port), "80");
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) {
        debug_log("getaddrinfo(%s) failed", host);
        return 0;
    }

    for (rp = res; rp; rp = rp->ai_next) {
        struct pollfd pfd;
        int rc;
        int soErr = 0;
        socklen_t soLen = sizeof(soErr);

        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        rc = connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen);
        if (rc == 0) break;
        if (errno != EINPROGRESS) { close(fd); fd = -1; continue; }

        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        rc = poll(&pfd, 1, CONNECT_TIMEOUT_MS);
        if (rc <= 0) { close(fd); fd = -1; continue; }
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soErr, &soLen) != 0 || soErr != 0) {
            close(fd);
            fd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        debug_log("connect(%s) failed within %d ms", host, CONNECT_TIMEOUT_MS);
        return 0;
    }

    {
        struct timeval tv;
        char req[512];
        size_t cap = sizeof(resp);
        size_t n = 0;

        tv.tv_sec = READ_TIMEOUT_MS / 1000;
        tv.tv_usec = (READ_TIMEOUT_MS % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        snprintf(req, sizeof(req),
                 "GET %s HTTP/1.0%sHost: %s%sUser-Agent: hardfetch/3.3%s"
                 "Connection: close%s%s",
                 path, CRLF, host, CRLF, CRLF, CRLF, CRLF);
        if (write(fd, req, strlen(req)) <= 0) { close(fd); return 0; }

        while (n + 1 < cap) {
            ssize_t got = read(fd, resp + n, cap - 1 - n);
            if (got <= 0) break;
            n += (size_t)got;
        }
        close(fd);
        resp[n] = 0;
        if (n == 0) return 0;
    }

    /* The body follows the blank line that ends the headers. */
    {
        char* body = strstr(resp, CRLF CRLF);
        size_t skip = 4;

        if (!body) { body = strstr(resp, "\n\n"); skip = 2; }
        if (!body) {
            debug_log("no header terminator in the %s response", host);
            return 0;
        }
        snprintf(out, outLen, "%s", body + skip);
    }

    {
        size_t len = strlen(out);
        while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' ||
                           out[len - 1] == ' ' || out[len - 1] == '\t'))
            out[--len] = 0;
    }

    {
        int dots = 0, digits = 0;
        size_t i, len = strlen(out);
        for (i = 0; i < len; i++) {
            if (out[i] == '.') dots++;
            else if (out[i] >= '0' && out[i] <= '9') digits++;
            else return 0;
        }
        if (dots != 3 || digits < 7) {
            debug_log("'%s' did not return an IPv4 literal", host);
            out[0] = 0;
            return 0;
        }
    }
    return 1;
}

static void* lookup_proc(void* arg)
{
    lookup* lk = (lookup*)arg;
    size_t i;

    for (i = 0; i < sizeof(kHosts) / sizeof(kHosts[0]); i++) {
        if (fetch_one(kHosts[i], kPaths[i], lk->buf, sizeof(lk->buf)))
            break;
        lk->buf[0] = 0;
    }
    lk->done = 1;
    return NULL;
}

static lookup    g_lookup;
static pthread_t g_thread;
static int       g_started;

void public_ip_start(hf_state* s, unsigned int timeoutMs)
{
    (void)s;
    (void)timeoutMs;

    debug_begin("publicip");
    if (g_started) return;
    g_started = 1;

    memset(&g_lookup, 0, sizeof(g_lookup));
    if (pthread_create(&g_thread, NULL, lookup_proc, &g_lookup) != 0) {
        g_started = 0;
        debug_log("could not start the lookup thread");
    } else {
        /* Deliberately not detached: public_ip_join waits on it with a
           deadline, and joining a detached thread returns immediately. */
        debug_log("lookup thread started");
    }
    debug_end();
}

int public_ip_join(hf_state* s, unsigned int waitMs)
{
    struct timespec deadline;
    int rc;

    debug_begin("publicip");
    if (!g_started) { debug_end(); return 0; }

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += waitMs / 1000;
    deadline.tv_nsec += (long)(waitMs % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }

    rc = pthread_timedjoin_np(g_thread, NULL, &deadline);
    if (rc != 0 || !g_lookup.done || !g_lookup.buf[0]) {
        debug_log("no public IP within %u ms", waitMs);
        s->ip_public[0] = 0;
        debug_end();
        return 0;
    }

    snprintf(s->ip_public, sizeof(s->ip_public), "%s", g_lookup.buf);
    debug_log("public IP = %s", s->ip_public);
    debug_end();
    return 1;
}
