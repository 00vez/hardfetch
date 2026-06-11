#include "gpu_nvidia.h"
#include "output.h"
#include "nvml.h"

#include <stdio.h>
#include <windows.h>

// Function pointer types
typedef nvmlReturn_t (*PFN_nvmlInit_v2)(void);
typedef nvmlReturn_t (*PFN_nvmlShutdown)(void);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetCount_v2)(unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetHandleByIndex_v2)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetName)(nvmlDevice_t, char*, unsigned int);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMaxClockInfo)(nvmlDevice_t, nvmlClockType_t, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMemoryInfo_v2)(nvmlDevice_t, nvmlMemory_v2_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetTemperature)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);

static struct {
    HMODULE dll;
    PFN_nvmlInit_v2                  pfnInit;
    PFN_nvmlShutdown                 pfnShutdown;
    PFN_nvmlDeviceGetCount_v2        pfnGetCount;
    PFN_nvmlDeviceGetHandleByIndex_v2 pfnGetHandle;
    PFN_nvmlDeviceGetName            pfnGetName;
    PFN_nvmlDeviceGetMaxClockInfo    pfnGetClock;
    PFN_nvmlDeviceGetUtilizationRates pfnGetUtil;
    PFN_nvmlDeviceGetMemoryInfo_v2   pfnGetMemV2;
    PFN_nvmlDeviceGetMemoryInfo      pfnGetMem;
    PFN_nvmlDeviceGetTemperature     pfnGetTemp;
    PFN_nvmlDeviceGetPowerUsage      pfnGetPower;
} nvml;

static int load_nvml(void)
{
    nvml.dll = LoadLibraryA("nvml.dll");
    if (!nvml.dll)
        return 0;

    nvml.pfnInit     = (PFN_nvmlInit_v2)GetProcAddress(nvml.dll, "nvmlInit_v2");
    nvml.pfnShutdown = (PFN_nvmlShutdown)GetProcAddress(nvml.dll, "nvmlShutdown");
    nvml.pfnGetCount = (PFN_nvmlDeviceGetCount_v2)GetProcAddress(nvml.dll, "nvmlDeviceGetCount_v2");
    nvml.pfnGetHandle = (PFN_nvmlDeviceGetHandleByIndex_v2)GetProcAddress(nvml.dll, "nvmlDeviceGetHandleByIndex_v2");
    nvml.pfnGetName  = (PFN_nvmlDeviceGetName)GetProcAddress(nvml.dll, "nvmlDeviceGetName");
    nvml.pfnGetClock = (PFN_nvmlDeviceGetMaxClockInfo)GetProcAddress(nvml.dll, "nvmlDeviceGetMaxClockInfo");
    nvml.pfnGetUtil  = (PFN_nvmlDeviceGetUtilizationRates)GetProcAddress(nvml.dll, "nvmlDeviceGetUtilizationRates");
    nvml.pfnGetMemV2 = (PFN_nvmlDeviceGetMemoryInfo_v2)GetProcAddress(nvml.dll, "nvmlDeviceGetMemoryInfo_v2");
    nvml.pfnGetMem   = (PFN_nvmlDeviceGetMemoryInfo)GetProcAddress(nvml.dll, "nvmlDeviceGetMemoryInfo");
    nvml.pfnGetTemp  = (PFN_nvmlDeviceGetTemperature)GetProcAddress(nvml.dll, "nvmlDeviceGetTemperature");
    nvml.pfnGetPower = (PFN_nvmlDeviceGetPowerUsage)GetProcAddress(nvml.dll, "nvmlDeviceGetPowerUsage");

    if (!nvml.pfnInit || !nvml.pfnShutdown || !nvml.pfnGetCount ||
        !nvml.pfnGetHandle || !nvml.pfnGetName) {
        FreeLibrary(nvml.dll);
        return 0;
    }

    return 1;
}

void print_gpu_info(void)
{
    if (!load_nvml()) {
        print_label("GPU");
        print_value("[NVIDIA driver not found]");
        print_newline();
        return;
    }

    if (nvml.pfnInit() != NVML_SUCCESS) {
        FreeLibrary(nvml.dll);
        print_label("GPU");
        print_value("[NVML init failed]");
        print_newline();
        return;
    }

    unsigned int count = 0;
    if (nvml.pfnGetCount(&count) != NVML_SUCCESS || count == 0) {
        nvml.pfnShutdown();
        FreeLibrary(nvml.dll);
        print_label("GPU");
        print_value("[No NVIDIA GPU found]");
        print_newline();
        return;
    }

    nvmlDevice_t device;
    if (nvml.pfnGetHandle(0, &device) != NVML_SUCCESS) {
        nvml.pfnShutdown();
        FreeLibrary(nvml.dll);
        print_label("GPU");
        print_value("[Failed to get GPU handle]");
        print_newline();
        return;
    }

    // Name
    char name[256] = {0};
    if (nvml.pfnGetName)
        nvml.pfnGetName(device, name, sizeof(name));

    print_label("GPU");
    print_value_green(name);
    print_newline();

    // Core clock, Mem clock, Utilization
    unsigned int coreClock = 0, memClock = 0, util = 0;
    if (nvml.pfnGetClock)
        nvml.pfnGetClock(device, NVML_CLOCK_GRAPHICS, &coreClock);
    if (nvml.pfnGetClock)
        nvml.pfnGetClock(device, NVML_CLOCK_MEM, &memClock);
    if (nvml.pfnGetUtil) {
        nvmlUtilization_t u;
        if (nvml.pfnGetUtil(device, &u) == NVML_SUCCESS)
            util = u.gpu;
    }

    printf("  \x1b[90mCore %u MHz  |  Mem %u MHz  |  Load %u%%\x1b[0m\n",
           coreClock, memClock, util);

    // VRAM
    double vramUsed = 0, vramTotal = 0;
    if (nvml.pfnGetMemV2) {
        nvmlMemory_v2_t mem = { .version = nvmlMemory_v2 };
        if (nvml.pfnGetMemV2(device, &mem) == NVML_SUCCESS) {
            vramUsed = mem.used / (1024.0 * 1024.0 * 1024.0);
            vramTotal = mem.total / (1024.0 * 1024.0 * 1024.0);
        }
    } else if (nvml.pfnGetMem) {
        nvmlMemory_t mem;
        if (nvml.pfnGetMem(device, &mem) == NVML_SUCCESS) {
            vramUsed = mem.used / (1024.0 * 1024.0 * 1024.0);
            vramTotal = mem.total / (1024.0 * 1024.0 * 1024.0);
        }
    }

    // Temperature
    unsigned int temp = 0;
    if (nvml.pfnGetTemp)
        nvml.pfnGetTemp(device, NVML_TEMPERATURE_GPU, &temp);

    // Power (milliwatts -> watts)
    unsigned int powerMw = 0;
    if (nvml.pfnGetPower)
        nvml.pfnGetPower(device, &powerMw);
    unsigned int powerW = powerMw / 1000;

    printf("  \x1b[90mVRAM %.1f / %.1f GB  |  Temp %u\xc2\xb0""C  |  Power %u W\x1b[0m\n",
           vramUsed, vramTotal, temp, powerW);

    nvml.pfnShutdown();
    FreeLibrary(nvml.dll);
}
