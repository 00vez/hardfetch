#ifndef HF_STATE_H
#define HF_STATE_H

#include <stddef.h>

#define HF_MAX_GPUS     8
#define HF_MAX_DISPLAYS 8

/* Everything hardfetch v0.3.0 knows about the machine.
   Split into static (collected once) and volatile (re-collected per --live tick). */

typedef struct {
    char name[160];
    int  integrated;          /* 1 = iGPU, 0 = discrete                */
    int  software;            /* software / indirect display adapter  */
    unsigned int domain, bus, device, func;
    unsigned int vendor_id, device_id;
    unsigned long long vram_total;   /* bytes, 0 if unknown */
    unsigned long long vram_used;
    unsigned int core_mhz, mem_mhz;
    unsigned int load_pct, temp_c, power_w;
    int  have_nvml;
} hf_gpu;

typedef struct {
    char name[64];
    unsigned int width, height;
    unsigned int refresh_num, refresh_den;
    unsigned int phys_w_mm, phys_h_mm;
    unsigned int dpi;
    int primary;
} hf_display;

typedef struct {
    /* ---- static ---- */
    char        user_name[128];
    char        host_name[192];

    char        os_product[128];
    char        os_display_version[32];
    char        os_arch[32];
    unsigned long os_build;      /* Windows: numeric CurrentBuildNumber */
    char        os_build_str[64]; /* POSIX: kernel release string        */

    unsigned long long build_ubr;
    char        kernel_name[32];
    unsigned int kernel_major, kernel_minor;
    unsigned long kernel_build;   /* Windows: numeric build            */
    char        kernel_build_str[64]; /* POSIX: kernel release string  */

    char        host[192];

    unsigned long long uptime_secs;

    char        shell_name[64];
    char        shell_version[32];

    char        term_name[64];
    char        term_scheme[48];
    char        term_color[32];
    char        term_font[80];
    int         term_cols, term_rows;

    unsigned long long ram_used, ram_total;
    unsigned int ram_mhz, ram_cl;
    char        ram_type[16];
    unsigned int ram_modules;
    unsigned long long ram_module_bytes;

    char        disk_volume[64];   /* 8 was too small: /home/user truncated to /home/u */
    unsigned long long disk_used, disk_total;

    hf_gpu      gpus[HF_MAX_GPUS];
    int         gpu_count;

    hf_display  displays[HF_MAX_DISPLAYS];
    int         display_count;

    /* ---- volatile ---- */
    char        cpu_name[128];
    unsigned int cpu_freq_mhz;
    unsigned int cpu_boost_mhz;
    unsigned int cpu_base_mhz;
    int         cpu_cores, cpu_threads;
    double      cpu_load;          /* 0..1 */
    int         cpu_temp_c;        /* -1 when no sensor */
    unsigned long long uptime_secs_live;

    /* ---- presentation ---- */
    int         options_all;

    /* ---- battery ---- */
    int          have_battery;
    unsigned int battery_pct;
    int          battery_charging;   /* 1 = charging on mains           */
    int          battery_temp_c;     /* -1 when the platform has no sensor */
    unsigned int battery_cycles;     /* 0 when unknown                  */

    /* ---- network ---- */
    char        ip_local[96];
    char        ip_label[24];
    char        ip_public[64];
    struct {
        char label[24];
        char value[48];
    } ip_ifaces[8];
    int         ip_iface_count;
} hf_state;

void hf_state_init(hf_state* s);

#endif