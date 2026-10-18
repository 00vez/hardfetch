#define WIN32_LEAN_AND_MEAN
#include "debug.h"
#include "hf_state.h"
#include "output.h"
#include "render.h"

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
#include <windows.h>

#ifdef __APPLE__
#include "posix/apple_smc.h"
#endif

#ifndef HF_VERSION
#define HF_VERSION "0.3.0"
#endif

/* Public-IP lookup is bounded so -a can never hang on a slow endpoint. */
#define PUBLIC_IP_TIMEOUT_MS 250
#define WORKER_DEADLINE_MS  3000
#define LIVE_DEFAULT_INTERVAL 1

struct Options {
    int show_all;
    int debug;
    int live;             /* 0 = off, 1 = on */
    unsigned int live_interval_s;
};

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

/* SetConsoleCtrlHandler only fires when the process owns a console. When
   stdout is redirected there is none, and Ctrl+C would never reach the loop,
   so the CRT signal handlers are installed as well. */
static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

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
        "  The public IP lookup needs internet access and is time limited.\n"
    );
}

/* Worker thread: everything that is slow or would otherwise serialise.
   Runs concurrently with the main thread's own collection. */
static DWORD WINAPI worker_proc(LPVOID arg)
{
    hf_state* s = (hf_state*)arg;

    /* GPU needs NVML initialisation, which is the slowest single step; start
       it first so its latency overlaps everything that follows. */
    gpu_collect(s);
    shell_collect(s);
    terminal_collect(s);
    display_collect(s);
    cpu_temp_collect(s);

    return 0;
}

static void collect_static(hf_state* s)
{
    os_collect(s);
    kernel_collect_build(s);
    kernel_collect_kernel(s);
    host_collect(s);
    uptime_collect(s);
    memory_collect_usage(s);
    storage_collect(s);
    ip_collect(s, s->options_all);
}

static void collect_live(hf_state* s)
{
    uptime_collect(s);
    cpu_collect_live(s);
    gpu_collect(s);
    if (s->options_all)
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
    HANDLE worker;

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

    worker = CreateThread(NULL, 0, worker_proc, &s, 0, NULL);

    collect_static(&s);
    cpu_collect_static(&s);
    memory_collect_spd(&s);   /* single WMI query on the main thread */

    if (o.show_all)
        public_ip_fetch(&s, PUBLIC_IP_TIMEOUT_MS);

    if (worker) {
        /* Bounded: a collector that blocks on a wedged WMI or display service
           must not be able to hang the tool. Whatever was collected so far
           still renders. */
        if (WaitForSingleObject(worker, WORKER_DEADLINE_MS) != WAIT_OBJECT_0) {
            debug_log("worker did not finish within %d ms, rendering partial data",
                      WORKER_DEADLINE_MS);
            TerminateThread(worker, 1);
        }
        CloseHandle(worker);
    }

    cpu_sample_end(&s);

    if (!o.live) {
        render(&s, o.show_all);
        return 0;
    }

    if (!stdout_is_console()) {
        /* Redrawing a file forever produces an unkillable loop that shows the
           user nothing. Print the single frame and stop. */
        fprintf(stderr, "--live needs a console; printing one frame instead");
        render(&s, o.show_all);
        return 0;
    }

    install_interrupt_handlers();
    output_hide_cursor();
    while (!g_stop) {
        ULONGLONG deadline = GetTickCount64() + (ULONGLONG)o.live_interval_s * 1000ULL;
        output_clear_screen();
        render(&s, o.show_all);
        fflush(stdout);

        while (!g_stop) {
            ULONGLONG now = GetTickCount64();
            if (now >= deadline) break;
            Sleep(now + 50 >= deadline ? (DWORD)(deadline - now) : 50);
        }
        if (g_stop) break;
        collect_live(&s);
    }
    output_show_cursor();
    return 0;
}