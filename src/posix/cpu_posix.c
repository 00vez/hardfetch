#define _GNU_SOURCE
#include "../cpu.h"
#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if defined(__APPLE__)
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#include <unistd.h>
#include "apple_pmgr.h"

static double cpu_usage_percent(unsigned interval_ms) {
  natural_t n1=0,n2=0; processor_info_array_t t1=NULL,t2=NULL;
  mach_msg_type_number_t c1=0,c2=0;
  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &n1, &t1, &c1)!=KERN_SUCCESS) return -1;
  usleep(interval_ms*1000);
  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &n2, &t2, &c2)!=KERN_SUCCESS){
    vm_deallocate(mach_task_self(),(vm_address_t)t1,c1*sizeof(integer_t)); return -1;
  }
  uint64_t b1=0,tot1=0,b2=0,tot2=0;
  for(natural_t i=0;i<n1 && i<n2;i++){
    integer_t *p1=t1+i*CPU_STATE_MAX; integer_t *p2=t2+i*CPU_STATE_MAX;
    uint64_t bb1=p1[CPU_STATE_USER]+p1[CPU_STATE_SYSTEM]+p1[CPU_STATE_NICE];
    uint64_t tt1=bb1+p1[CPU_STATE_IDLE];
    uint64_t bb2=p2[CPU_STATE_USER]+p2[CPU_STATE_SYSTEM]+p2[CPU_STATE_NICE];
    uint64_t tt2=bb2+p2[CPU_STATE_IDLE];
    b1+=bb1; tot1+=tt1; b2+=bb2; tot2+=tt2;
  }
  vm_deallocate(mach_task_self(),(vm_address_t)t1,c1*sizeof(integer_t));
  vm_deallocate(mach_task_self(),(vm_address_t)t2,c2*sizeof(integer_t));
  uint64_t dt=tot2-tot1, db=b2-b1;
  return dt? (double)db/(double)dt*100.0 : -1;
}
#endif

static void trim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t'))
        s[--n] = '\0';
}

static void parse_after(const char* line, const char* key, char* out, size_t n)
{
    out[0] = '\0';
    const char* p = strstr(line, key);
    if (!p) return;
    p += strlen(key);
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    strncpy(out, p, n - 1);
    out[n - 1] = '\0';
    trim(out);
}

static double read_load(void)
{
    static long long prev_idle, prev_total;
    static int first = 1;
    double load = 0;

    FILE* f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char line[512];
    if (fgets(line, sizeof(line), f)) {
        long long user=0, nice=0, sys=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
        sscanf(line, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
               &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
        long long total = user + nice + sys + idle + iowait + irq + softirq + steal;
        long long idl   = idle + iowait;
        if (!first) {
            long long dtotal = total - prev_total;
            long long didle  = idl   - prev_idle;
            if (dtotal > 0) load = (1.0 - (double)didle / (double)dtotal) * 100.0;
        }
        prev_total = total; prev_idle = idl; first = 0;
    }
    fclose(f);
    return load;
}

void print_cpu_info(void)
{
    char name[256] = "Unknown CPU";
    double mhz = 0;
    double mhz_e = 0;
    int logical = 0;

#if defined(__APPLE__)
    {
        int got_name = 0;
        (void)got_name;
        char brand[256] = {0};
        size_t len = sizeof(brand);
        if (sysctlbyname("machdep.cpu.brand_string", brand, &len, NULL, 0) == 0) {
            strncpy(name, brand, sizeof(name)-1); name[sizeof(name)-1]='\0';
            got_name = 1;
        }
        int ncpu = 0;
        len = sizeof(ncpu);
        sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);
        logical = ncpu;
        uint32_t p_mhz = appleMaxFreqMHz("voltage-states5-sram");
        uint32_t e_mhz = appleMaxFreqMHz("voltage-states1-sram");
        if (p_mhz > 0) mhz = (double)p_mhz;
        if (e_mhz > 0) mhz_e = (double)e_mhz;
        /* E-Core shown separately if needed via output formatting below */
    }
#else
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        int got_name = 0;
        while (fgets(line, sizeof(line), f)) {
            char val[256];
            if (!got_name) { parse_after(line, "model name", val, sizeof(val)); if (val[0]) { strncpy(name, val, sizeof(name)-1); name[sizeof(name)-1]='\0'; got_name = 1; } }
            parse_after(line, "cpu MHz", val, sizeof(val));
            if (val[0]) mhz = strtod(val, NULL);
            if (strncmp(line, "processor", 9) == 0) logical++;
        }
        fclose(f);
    }
#endif

    char* suffix = strstr(name, "-Core");
    if (suffix) {
        char* bp = suffix - 1;
        while (bp > name && *bp >= '0' && *bp <= '9') bp--;
        if (*bp == ' ') *bp = '\0';
    }

    (void)parse_after;
    double load = -1;
#if defined(__APPLE__)
    load = cpu_usage_percent(200);
#else
    double s1 = read_load();
    struct timespec ts = { 0, 20 * 1000 * 1000 };
    nanosleep(&ts, NULL);
    double s2 = read_load();
    (void)s1;
    load = s2;
#endif

    char mainLine[320];
    int pos = snprintf(mainLine, sizeof(mainLine), "%s (%d)", name, logical);
    if (mhz > 0) pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " @ %.2f GHz", mhz / 1000.0);
    if (mhz_e > 0) pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " E %.2f GHz", mhz_e / 1000.0);
    print_block_green("CPU", mainLine);

    print_cpu_temp_power(load);
}
