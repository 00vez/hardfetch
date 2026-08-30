#define _GNU_SOURCE
#include "../public_ip.h"
#include "../output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cond = PTHREAD_COND_INITIALIZER;
static char            g_ip[64];
static int             g_done = 0;
static int             g_started = 0;

static const char* kHost = "ifconfig.me";
static const char* kPath = "/ip";

static void* fetch_proc(void* arg)
{
    (void)arg;
    int fd = -1;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = NULL;

    if (getaddrinfo(kHost, "80", &hints, &res) == 0 && res) {
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd >= 0) {
            struct timeval tv = { 3, 0 };
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            if (connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
                char req[256];
                int rl = snprintf(req, sizeof(req),
                    "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                    kPath, kHost);
                if (send(fd, req, rl, 0) == rl) {
                    char buf[1024];
                    int total = 0, n;
                    while (total < (int)sizeof(buf) - 1 &&
                           (n = recv(fd, buf + total, sizeof(buf) - 1 - total, 0)) > 0)
                        total += n;
                    buf[total] = '\0';
                    char* body = strstr(buf, "\r\n\r\n");
                    if (body) {
                        body += 4;
                        size_t l = strlen(body);
                        while (l > 0 && (body[l-1] == '\n' || body[l-1] == '\r' ||
                                          body[l-1] == ' '  || body[l-1] == '\t'))
                            body[--l] = '\0';
                        pthread_mutex_lock(&g_lock);
                        size_t cp = l < sizeof(g_ip) - 1 ? l : sizeof(g_ip) - 1;
                        memcpy(g_ip, body, cp);
                        g_ip[cp] = '\0';
                        pthread_mutex_unlock(&g_lock);
                    }
                }
            }
        }
        freeaddrinfo(res);
    }
    if (fd >= 0) close(fd);

    pthread_mutex_lock(&g_lock);
    g_done = 1;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

void public_ip_start(void)
{
    pthread_mutex_lock(&g_lock);
    if (g_started) { pthread_mutex_unlock(&g_lock); return; }
    g_started = 1;
    g_done = 0;
    g_ip[0] = '\0';
    pthread_mutex_unlock(&g_lock);

    pthread_t t;
    if (pthread_create(&t, NULL, fetch_proc, NULL) == 0)
        pthread_detach(t);
}

void public_ip_print(void)
{
    pthread_mutex_lock(&g_lock);

    if (!g_started) {
        pthread_mutex_unlock(&g_lock);
        print_block("Public", "N/A");
        return;
    }
    if (!g_done) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&g_cond, &g_lock, &ts);
    }

    int ready = g_done;
    char ip[64];
    ip[0] = '\0';
    if (ready) {
        strncpy(ip, g_ip, sizeof(ip) - 1);
        ip[sizeof(ip) - 1] = '\0';
    }
    pthread_mutex_unlock(&g_lock);

    if (ready && ip[0]) print_block("Public", ip);
    else                print_block("Public", "N/A");
}
