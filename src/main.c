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
#include "network.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <lmcons.h>

#define VERSION "0.1.0"

static void print_help(void)
{
    printf(
        "hardfetch v" VERSION " - compact Windows system info\n"
        "\n"
        "Usage: hardfetch [options]\n"
        "\n"
        "Options:\n"
        "  --net      Show network throughput and ping\n"
        "  --version  Print version and exit\n"
        "  --help     Show this help and exit\n"
        "\n"
        "Notes:\n"
        "  ICMP ping (--net), disk temperature, and CPU power\n"
        "  require administrator privileges.\n"
        "  x86-64 only.\n"
    );
}

static void print_user_host(void)
{
    char user[UNLEN + 1] = {0};
    DWORD userLen = sizeof(user);
    if (!GetUserNameA(user, &userLen))
        strcpy_s(user, sizeof(user), "user");

    char host[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD hostLen = sizeof(host);
    if (!GetComputerNameA(host, &hostLen))
        strcpy_s(host, sizeof(host), "unknown");

    char buf[512];
    snprintf(buf, sizeof(buf), "%s@%s", user, host);
    print_header(buf);
}

int main(int argc, char* argv[])
{
    int show_net = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("hardfetch v" VERSION "\n");
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--net") == 0) {
            show_net = 1;
        }
    }

    output_init();

    if (show_net)
        net_start_async();

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
        net_wait();
        net_print();
        print_newline();
    }

    return 0;
}