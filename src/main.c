#include "output.h"
#include "os_info.h"
#include "host.h"
#include "kernel.h"
#include "uptime.h"
#include "shell.h"
#include "gpu.h"
#include "cpu.h"
#include "memory.h"
#include "storage.h"
#include "ip_addr.h"
#include "public_ip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#else
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX MAXHOSTNAMELEN
#endif
#endif

#define VERSION "0.2.0"

static void print_help(void)
{
    printf(
        "hardfetch v" VERSION " - compact system info\n"
        "\n"
        "Usage: hardfetch [options]\n"
        "\n"
        "Options:\n"
        "  -n, --net  Show network section (interfaces + public IP)\n"
        "  -v, --version  Print version and exit\n"
        "  -h, --help     Show this help and exit\n"
        "\n"
        "Notes:\n"
        "  Public IP lookup requires internet access.\n"
        "  x86-64 only.\n"
    );
}

static void print_user_host(void)
{
    char user[128] = "user";
    char host[256] = "unknown";
#ifdef _WIN32
    char u[UNLEN + 1] = {0};
    DWORD ul = sizeof(u);
    if (GetUserNameA(u, &ul)) {
        strncpy_s(user, sizeof(user), u, _TRUNCATE);
    }
    char h[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD hl = sizeof(h);
    if (GetComputerNameA(h, &hl)) {
        strncpy_s(host, sizeof(host), h, _TRUNCATE);
    }
#else
    const char* u = getenv("USER");
    if (u && *u) {
        strncpy(user, u, sizeof(user) - 1);
        user[sizeof(user) - 1] = '\0';
    }
    char h[HOST_NAME_MAX + 1] = {0};
    if (gethostname(h, sizeof(h) - 1) == 0) {
        strncpy(host, h, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }
#endif
    char buf[512];
    snprintf(buf, sizeof(buf), "%s@%s", user, host);
    print_header(buf);
}

int main(int argc, char* argv[])
{
    int show_net = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("hardfetch v" VERSION "\n");
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--net") == 0 || strcmp(argv[i], "-n") == 0) {
            show_net = 1;
        }
    }

    output_init();

    if (show_net)
        public_ip_start();

    print_header("hardfetch v" VERSION);
    print_user_host();
    print_newline();

    print_os_info();
    print_host_info();
    print_kernel_info();
    print_uptime_info();
    print_shell_info();
    print_newline();

    print_cpu_info();
    print_memory_info();
    print_newline();

    print_gpu_info();
    print_newline();

    print_storage_info();
    print_newline();

    if (show_net) {
        print_ip_info();
        public_ip_print();
        print_newline();
    }

    return 0;
}
