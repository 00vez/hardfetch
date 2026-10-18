#include "render.h"
#include "fmt.h"
#include "output.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/* Joins up to three fields with the separator, skipping empty ones. The result
   starts at the first character: no leading separator, no padding. */
static int join_fields(char* out, size_t n, const char* a, const char* b, const char* c)
{
    const char* parts[3];
    const char* sep = output_separator();
    size_t pos = 0;
    int used = 0;
    int i;

    parts[0] = (a && a[0]) ? a : NULL;
    parts[1] = (b && b[0]) ? b : NULL;
    parts[2] = (c && c[0]) ? c : NULL;

    out[0] = 0;
    for (i = 0; i < 3; i++) {
        int w;
        if (!parts[i]) continue;

        if (used) {
            w = snprintf(out + pos, n - pos, " %s ", sep);
            if (w < 0) break;
            pos += (size_t)w;
        }

        w = snprintf(out + pos, n - pos, "%s", parts[i]);
        if (w < 0) break;
        pos += (size_t)w;
        used++;
    }
    return used;
}

static void render_header(void)
{
    char user[128] = "user";
    char host[192] = "unknown";
    char line[512];

    {
        wchar_t wu[256];
        DWORD n = (DWORD)(sizeof(wu) / sizeof(wu[0]));
        if (GetUserNameW(wu, &n))
            WideCharToMultiByte(CP_UTF8, 0, wu, -1, user, (int)sizeof(user), NULL, NULL);
    }
    {
        wchar_t wh[256];
        DWORD n = (DWORD)(sizeof(wh) / sizeof(wh[0]));
        if (GetComputerNameExW(ComputerNameDnsHostname, wh, &n) && wh[0])
            WideCharToMultiByte(CP_UTF8, 0, wh, -1, host, (int)sizeof(host), NULL, NULL);
        else if (GetComputerNameW(wh, &n) && wh[0])
            WideCharToMultiByte(CP_UTF8, 0, wh, -1, host, (int)sizeof(host), NULL, NULL);
    }

    snprintf(line, sizeof(line), "hardfetch v" HF_VERSION " %s %s@%s",
             output_separator(), user, host);
    print_header(line);
    print_newline();
}

static void render_os(const hf_state* s)
{
    char line[256];
    char product[160];
    char build[64];
    char kernel[96];
    char ulen[64];

    snprintf(product, sizeof(product), "%s %s", s->os_product, s->os_display_version);
    join_fields(line, sizeof(line), product, s->os_arch, NULL);
    print_row("OS", line);

    if (s->options_all) {
        snprintf(build, sizeof(build), "%lu.%llu", s->os_build, s->build_ubr);
        print_row("Build", build);

        snprintf(kernel, sizeof(kernel), "%s %u.%u.%lu.%llu",
                 s->kernel_name, s->kernel_major, s->kernel_minor,
                 s->kernel_build, s->build_ubr);
        print_row("Kernel", kernel);
    }

    print_row("Host", s->host);

    if (s->options_all) {
        if (s->shell_name[0]) {
            join_fields(line, sizeof(line), s->shell_name, s->shell_version, NULL);
            print_row("Shell", line);
        }

        if (s->term_name[0]) {
            char size[32] = "";
            char detail[192];

            if (s->term_cols > 0 && s->term_rows > 0)
                snprintf(size, sizeof(size), "%dx%d", s->term_cols, s->term_rows);

            join_fields(detail, sizeof(detail), s->term_name, size, s->term_color);
            print_row("Terminal", detail);

            if (s->term_font[0]) print_row_detail(s->term_font);
            if (s->term_scheme[0]) {
                char scheme[64];
                snprintf(scheme, sizeof(scheme), "scheme %s", s->term_scheme);
                print_row_detail(scheme);
            }
        }
    }

    fmt_uptime(ulen, sizeof(ulen),
               s->uptime_secs_live ? s->uptime_secs_live : s->uptime_secs);
    print_row("Uptime", ulen);
}

static void render_cpu(const hf_state* s)
{
    char clock[24], load[16], temp[24], detail[128];

    print_row_green("CPU", s->cpu_name);

    fmt_freq(clock, sizeof(clock), s->cpu_freq_mhz);
    fmt_load(load, sizeof(load), s->cpu_load, "Load");
    if (s->cpu_temp_c > 0)
        fmt_temp(temp, sizeof(temp), s->cpu_temp_c);
    else
        temp[0] = 0;

    join_fields(detail, sizeof(detail), clock, load, temp);
    print_row_detail(detail);

    if (s->options_all && (s->cpu_boost_mhz > 0 || s->cpu_threads > 0)) {
        char boost[32] = "";
        char cores[32] = "";
        char extra[128];

        if (s->cpu_boost_mhz > 0) {
            char f[24];
            fmt_freq(f, sizeof(f), s->cpu_boost_mhz);
            snprintf(boost, sizeof(boost), "boost %s", f);
        }
        if (s->cpu_threads > 0) {
            if (s->cpu_cores > 0 && s->cpu_cores != s->cpu_threads)
                snprintf(cores, sizeof(cores), "%dC/%dT", s->cpu_cores, s->cpu_threads);
            else
                snprintf(cores, sizeof(cores), "%d threads", s->cpu_threads);
        }
        join_fields(extra, sizeof(extra), boost, cores, NULL);
        if (extra[0]) print_row_detail(extra);
    }
}

static void render_memory(const hf_state* s)
{
    char amounts[48], pct[32], line[96], detail[96];
    char speed[32] = "";
    char cas[16] = "";

    fmt_bytes_pair(amounts, sizeof(amounts), s->ram_used, s->ram_total);
    fmt_load(pct, sizeof(pct),
             s->ram_total ? (double)s->ram_used / (double)s->ram_total : 0.0, NULL);
    join_fields(line, sizeof(line), amounts, pct, NULL);
    print_row("RAM", line);

    if (s->ram_mhz > 0)
        snprintf(speed, sizeof(speed), "%u MHz", s->ram_mhz);
    if (s->ram_cl > 0)
        snprintf(cas, sizeof(cas), "CL%u", s->ram_cl);
    join_fields(detail, sizeof(detail), speed, cas, NULL);
    if (detail[0]) print_row_detail(detail);

    if (s->options_all && s->ram_type[0]) {
        char modules[48] = "";
        char one[32] = "";
        char extra[96];
        if (s->ram_module_bytes > 0) {
            fmt_bytes(one, sizeof(one), s->ram_module_bytes);
            if (s->ram_modules > 1)
                snprintf(modules, sizeof(modules), "%ux%s", s->ram_modules, one);
            else
                snprintf(modules, sizeof(modules), "%s", one);
        }
        join_fields(extra, sizeof(extra), s->ram_type, modules, NULL);
        if (extra[0]) print_row_detail(extra);
    }
}

static void render_gpu_one(const hf_gpu* g, const char* label)
{
    print_row_green(label, g->name);

    if (!g->have_nvml) {
        if (g->vram_total > 0) {
            char t[24], line[64];
            fmt_bytes(t, sizeof(t), g->vram_total);
            snprintf(line, sizeof(line), "VRAM %s", t);
            print_row_detail(line);
        }
        return;
    }

    {
        char load[24], temp[24], power[16], line[96];
        fmt_load(load, sizeof(load), (double)g->load_pct / 100.0, "Load");
        fmt_temp(temp, sizeof(temp), (int)g->temp_c);
        snprintf(power, sizeof(power), "%u W", g->power_w);
        join_fields(line, sizeof(line), load, temp, power);
        print_row_detail(line);
    }

    if (g->vram_total > 0) {
        char vram[64], core[40] = "", mem[40] = "", line[160];
        char tmp[40];

        fmt_bytes_pair(vram, sizeof(vram), g->vram_used, g->vram_total);
        if (g->core_mhz > 0) {
            fmt_freq(tmp, sizeof(tmp), g->core_mhz);
            snprintf(core, sizeof(core), "Core %s", tmp);
        }
        if (g->mem_mhz > 0) {
            fmt_freq(tmp, sizeof(tmp), g->mem_mhz);
            snprintf(mem, sizeof(mem), "Memory %s", tmp);
        }

        {
            char used[32] = "";
            fmt_load(used, sizeof(used),
                     (double)g->vram_used / (double)g->vram_total, NULL);
            join_fields(line, sizeof(line), vram, used, NULL);
        }
        {
            char vramLine[96];
            snprintf(vramLine, sizeof(vramLine), "VRAM %s", line);
            print_row_detail(vramLine);
        }

        if (core[0] || mem[0]) {
            join_fields(line, sizeof(line), core, mem, NULL);
            print_row_detail(line);
        }
    }
}

static void render_gpu(const hf_state* s)
{
    int primary_shown = 0;

    for (int i = 0; i < s->gpu_count; i++) {
        const hf_gpu* g = &s->gpus[i];
        char label[16];

        if (g->integrated) {
            if (!s->options_all) continue;
            snprintf(label, sizeof(label), "GPU %d", i + 1);
        } else {
            /* Discrete adapters carry the unnumbered GPU label. */
            snprintf(label, sizeof(label), "GPU");
        }

        if (primary_shown) print_newline();
        render_gpu_one(g, label);
        primary_shown = 1;
    }
}

static void render_disk(const hf_state* s)
{
    char amounts[48], pct[32], line[96];

    if (s->disk_total == 0) return;
    fmt_bytes_pair(amounts, sizeof(amounts), s->disk_used, s->disk_total);
    fmt_load(pct, sizeof(pct), (double)s->disk_used / (double)s->disk_total, NULL);
    join_fields(line, sizeof(line), amounts, pct, NULL);
    print_row("Disk", line);
}

static void render_display(const hf_state* s)
{
    for (int i = 0; i < s->display_count; i++) {
        const hf_display* d = &s->displays[i];
        char label[24];
        char resolution[32], hz[24], size[40], line[160];
        char dpi[24] = "";
        char extra[128];

        if (s->display_count > 1)
            snprintf(label, sizeof(label), "Display %d", i + 1);
        else
            snprintf(label, sizeof(label), "Display");

        snprintf(resolution, sizeof(resolution), "%ux%u", d->width, d->height);

        if (d->refresh_den > 0) {
            unsigned int hzValue = (unsigned int)
                (((unsigned long long)d->refresh_num * 1000ULL + d->refresh_den / 2) /
                 d->refresh_den);
            snprintf(hz, sizeof(hz), "%u Hz", hzValue);
        } else {
            hz[0] = 0;
        }

        if (d->phys_w_mm > 0 && d->phys_h_mm > 0) {
            unsigned long long sq = (unsigned long long)d->phys_w_mm * d->phys_w_mm +
                                    (unsigned long long)d->phys_h_mm * d->phys_h_mm;
            unsigned long long root = 0;
            while ((root + 1) * (root + 1) <= sq) root++;
            if (root / 25.4 > 1.0)
                snprintf(size, sizeof(size), "%u%c",
                         (unsigned int)((double)root / 25.4 + 0.5), (char)0x22);
            else
                snprintf(size, sizeof(size), "%u x %u mm", d->phys_w_mm, d->phys_h_mm);
        } else {
            size[0] = 0;
        }

        join_fields(line, sizeof(line), resolution, hz, size);
        print_row(label, line);

        if (d->dpi > 0)
            snprintf(dpi, sizeof(dpi), "%u DPI", d->dpi);
        join_fields(extra, sizeof(extra), d->name, dpi, NULL);
        if (extra[0]) print_row_detail(extra);
    }
}

static void render_network(const hf_state* s)
{
    char label[40];

    if (s->ip_local[0]) {
        snprintf(label, sizeof(label), "IP (%s)", s->ip_label);
        print_row(label, s->ip_local);
    }

    if (!s->options_all)
        return;

    snprintf(label, sizeof(label), "IP (Public)");
    print_row(label, s->ip_public[0] ? s->ip_public : "N/A");

    /* The remaining adapters follow the public IP, one per row, name left
       aligned in the value column like every other detail line. ip_collect
       already put them in alphabetical order. */
    if (s->ip_local[0]) {
        for (int i = 0; i < s->ip_iface_count; i++) {
            char line[192];
            snprintf(line, sizeof(line), "%s  %s",
                     s->ip_ifaces[i].label, s->ip_ifaces[i].value);
            print_row_detail(line);
        }
    }
}

void render(const hf_state* s, int options)
{
    (void)options;

    render_header();
    render_os(s);
    print_newline();

    render_cpu(s);
    print_newline();

    render_memory(s);
    print_newline();

    render_gpu(s);
    render_disk(s);
    render_display(s);
    render_network(s);
}