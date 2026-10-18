#define _GNU_SOURCE
#include "../public_ip.h"
#include "../debug.h"

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static const char* kHosts[] = { "api.ipify.org", "ifconfig.me", "icanhazip.com" };
static const char* kPaths[] = { "/", "/ip", "/" };

#define CRLF "\r\n"

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
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        debug_log("connect(%s) failed", host);
        return 0;
    }

    {
        struct timeval tv;
        char req[512];
        size_t cap = sizeof(resp);
        size_t n = 0;

        tv.tv_sec = 3;
        tv.tv_usec = 0;
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

void public_ip_fetch(hf_state* s, unsigned int timeoutMs)
{
    size_t i;

    (void)timeoutMs;
    debug_begin("publicip");
    s->ip_public[0] = 0;

    for (i = 0; i < sizeof(kHosts) / sizeof(kHosts[0]); i++) {
        if (fetch_one(kHosts[i], kPaths[i], s->ip_public, sizeof(s->ip_public))) {
            debug_log("public IP from '%s' = %s", kHosts[i], s->ip_public);
            break;
        }
        s->ip_public[0] = 0;
    }

    if (!s->ip_public[0])
        debug_log("no public IP resolved");

    debug_end();
}
