#define _GNU_SOURCE
#include "debug.h"
#include "version.h"
#include "hf_state.h"
#include "output.h"
#include "render.h"

#include "battery.h"
#include "cpu.h"
#include "cpu_temp.h"
#include "display.h"
#include "gpu.h"
#include "host.h"
#include "ip_addr.h"
#include "kernel.h"
#include "memory.h"
#include "os_info.h"
#include "public_ip.h"
#include "shell.h"
#include "storage.h"
#include "terminal.h"
#include "uptime.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE hf_thread;
#define HF_THREAD_RETURN DWORD WINAPI
#else
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "posix/thread_posix.h"
typedef hf_thread_posix hf_thread;
#define HF_THREAD_RETURN void*
#endif

#ifdef __APPLE__
#include "posix/apple_smc.h"
#endif

#define PUBLIC_IP_TIMEOUT_MS  250
#define PUBLIC_IP_DEADLINE_MS 600
#define WORKER_DEADLINE_MS  3000
#define LIVE_DEFAULT_INTERVAL 1

struct Options {
    int show_all;
    int debug;
    int live;
    unsigned int live_interval_s;
};

#ifdef _WIN32
static void worker_proc(void* arg);

static volatile LONG g_stop = 0;

static BOOL WINAPI on_console_event(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
        type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT ||
        type == CTRL_SHUTDOWN_EVENT) {
        g_stop = 1;
        return TRUE;
    }
    return FALSE;
}

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void install_interrupt_handlers(void)
{
    SetConsoleCtrlHandler(on_console_event, TRUE);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGBREAK, on_signal);
}

static int stdout_is_console(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    return h != INVALID_HANDLE_VALUE && h != NULL && GetConsoleMode(h, &mode) != 0;
}

static HF_THREAD_RETURN worker_entry(LPVOID arg)
{
    worker_proc(arg);
    return 0;
}

static int thread_start(hf_thread* t, void (*fn)(void*), void* arg)
{
    (void)fn;
    *t = CreateThread(NULL, 0, worker_entry, arg, 0, NULL);
    return *t != NULL;
}

/* Bounded: a collector that blocks on a wedged service must not be able to
   hang the tool. Whatever was collected so far still renders. */
static int thread_join_bounded(hf_thread* t)
{
    if (WaitForSingleObject(*t, WORKER_DEADLINE_MS) != WAIT_OBJECT_0) {
        debug_log("worker did not finish within %d ms, rendering partial data",
                  WORKER_DEADLINE_MS);
        TerminateThread(*t, 1);
        return 0;
    }
    return 1;
}

static void thread_close(hf_thread* t) { CloseHandle(*t); }

static int thread_valid(hf_thread* t) { return *t != NULL; }

static unsigned long long now_ms(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (unsigned long long)(c.QuadPart * 1000 / freq.QuadPart);
}

static void sleep_ms(unsigned int ms) { Sleep(ms); }

#else /* POSIX */

#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static void worker_proc(void* arg);

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void install_interrupt_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

static int stdout_is_console(void)
{
    struct termios t;
    return isatty(STDOUT_FILENO) && tcgetattr(STDOUT_FILENO, &t) == 0;
}

static HF_THREAD_RETURN worker_entry(void* arg)
{
    worker_proc(arg);
    return NULL;
}

static int thread_start(hf_thread* t, void (*fn)(void*), void* arg)
{
    (void)fn;
    return hf_thread_posix_start(t, worker_entry, arg);
}

static int thread_join_bounded(hf_thread* t)
{
    if (!hf_thread_posix_wait(t, WORKER_DEADLINE_MS)) {
        debug_log("worker did not finish within %d ms, rendering partial data",
                  WORKER_DEADLINE_MS);
        return 0;
    }
    return 1;
}

static void thread_close(hf_thread* t) { hf_thread_posix_close(t); }

static int thread_valid(hf_thread* t) { return t->started; }

static unsigned long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

static void sleep_ms(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

#endif

static void print_version(void)
{
    printf("hardfetch v" HF_VERSION "\n");
}

static void print_help(void)
{
    printf(
        "hardfetch v" HF_VERSION " - compact system info\n"
        "\n"
        "Usage: hardfetch [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help          Show this help and exit\n"
        "  -v, --version       Print version and exit\n"
        "  -d, --debug         Raw values, chosen sources and timings on stderr\n"
        "  -l, --live [secs]   Refresh the standard view; no value means every 1 s\n"
        "  -a, --all           Standard view plus build, kernel, shell, terminal,\n"
        "                      iGPU, display, all interfaces and public IP\n"
        "\n"
        "Notes:\n"
        "  --smc-dump dumps Apple SMC sensors (macOS only)\n"
        "  In --live, q, Esc or Ctrl+C quits.\n"
        "  The public IP lookup needs internet access and is time limited.\n"
    );
}

/* Worker thread: the collectors that are slow or would otherwise serialise.
   Runs concurrently with the main thread's own collection. */
static void worker_proc(void* arg)
{
    hf_state* s = (hf_state*)arg;

    /* GPU needs NVML initialisation, the slowest single step, so it goes
       first and its latency overlaps everything after it. */
    gpu_collect(s);
    shell_collect(s);
    terminal_collect(s);
    display_collect(s);
}

static void collect_static(hf_state* s)
{
    os_collect(s);
    kernel_collect_build(s);
    kernel_collect_kernel(s);
    host_collect(s);
    uptime_collect(s);
    battery_collect(s);
    memory_collect_usage(s);
    storage_collect(s);
    ip_collect(s, s->options_all);
}

static void collect_live(hf_state* s)
{
    uptime_collect(s);
    cpu_collect_live(s);
    gpu_collect(s);
    cpu_temp_collect(s);
}

static int parse_options(int argc, char** argv, struct Options* o, int* exit_code)
{
    int i;

    memset(o, 0, sizeof(*o));
    o->live_interval_s = LIVE_DEFAULT_INTERVAL;
    *exit_code = 0;

    for (i = 1; i < argc; i++) {
        const char* a = argv[i];

        if (!strcmp(a, "--version") || !strcmp(a, "-v")) { print_version(); return 0; }
        if (!strcmp(a, "--help") || !strcmp(a, "-h")) { print_help(); return 0; }

        if (!strcmp(a, "--debug") || !strcmp(a, "-d")) { o->debug = 1; continue; }
        if (!strcmp(a, "--all") || !strcmp(a, "-a")) { o->show_all = 1; continue; }

        if (!strcmp(a, "--live") || !strcmp(a, "-l")) {
            o->live = 1;
            /* Optional argument: only accept it if it is a bare number. */
            if (i + 1 < argc) {
                char* end = NULL;
                unsigned long v = strtoul(argv[i + 1], &end, 10);
                if (end && *end == '\0' && end != argv[i + 1] && v > 0 && v <= 3600) {
                    o->live_interval_s = (unsigned int)v;
                    i++;
                }
            }
            continue;
        }

        if (!strncmp(a, "--live=", 7)) {
            char* end = NULL;
            unsigned long v = strtoul(a + 7, &end, 10);
            if (end && *end == '\0' && v > 0 && v <= 3600) {
                o->live = 1;
                o->live_interval_s = (unsigned int)v;
                continue;
            }
        }

        if (!strcmp(a, "--smc-dump")) {
#ifdef __APPLE__
            apple_smc_dump(stdout);
#else
            fprintf(stderr, "--smc-dump only works on macOS\n");
#endif
            return 0;
        }

        fprintf(stderr, "unknown option: %s\n", a);
        fprintf(stderr, "try 'hardfetch --help'\n");
        *exit_code = 1;
        return 0;
    }
    return 1;
}

int main(int argc, char** argv)
{
    struct Options o;
    int exit_code = 0;
    hf_state s;
    hf_thread worker;

    if (!parse_options(argc, argv, &o, &exit_code))
        return exit_code;

    if (o.debug)
        debug_enable();

    output_init();
    hf_state_init(&s);
    s.options_all = o.show_all;

    debug_log("options: all=%d debug=%d live=%d interval=%us",
              o.show_all, o.debug, o.live, o.live_interval_s);

    /* The load measurement window spans the whole collection, so the wall
       clock cost of sampling is whatever the collection already takes. */
    cpu_sample_begin();

    thread_start(&worker, worker_proc, &s);

    ident_collect(&s);
    collect_static(&s);
    cpu_collect_static(&s);
    memory_collect_spd(&s);
    cpu_temp_collect(&s);

    /* Started before the collection so it overlaps with it. Nothing waits
       for it beyond a bounded join right before the first frame. */
    if (o.show_all)
        public_ip_start(&s, PUBLIC_IP_TIMEOUT_MS);

    if (thread_valid(&worker)) {
        thread_join_bounded(&worker);
        thread_close(&worker);
    }

    cpu_sample_end(&s);

    if (o.show_all)
        public_ip_join(&s, PUBLIC_IP_DEADLINE_MS);

    if (!o.live) {
        render(&s, o.show_all);
        return 0;
    }

    if (!stdout_is_console()) {
        /* Redrawing a file forever produces a loop the user cannot stop and
           that shows nothing. Print the single frame and stop. */
        fprintf(stderr, "--live needs a console; printing one frame instead\n");
        render(&s, o.show_all);
        return 0;
    }

    install_interrupt_handlers();
    output_hide_cursor();
    while (!g_stop) {
        unsigned long long deadline = now_ms() + (unsigned long long)o.live_interval_s * 1000ULL;

        output_clear_screen();
        render(&s, o.show_all);
        fflush(stdout);

        while (!g_stop) {
            unsigned long long n = now_ms();
            unsigned int remaining;
            if (output_quit_requested()) { g_stop = 1; break; }
            if (n >= deadline) break;
            remaining = (n + 50 >= deadline) ? (unsigned int)(deadline - n) : 50;
            /* Short slices so q is noticed within a fraction of the interval. */
            sleep_ms(remaining < 50 ? remaining : 50);
        }
        if (g_stop) break;
        collect_live(&s);
    }
    output_show_cursor();
    return 0;
}
