#include "gpu.h"
#include "output.h"

#include <cstdio>
#include <cstring>
#include <windows.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// --- Minimal NVML dynamic loading ---
typedef enum { NVML_SUCCESS = 0 } nvmlReturn_t;
struct nvmlDevice;
typedef struct nvmlDevice* nvmlDevice_t;
typedef enum { NVML_TEMPERATURE_GPU = 0 } nvmlTemperatureSensors_t;
typedef struct { unsigned int version; unsigned long long total; unsigned long long free; unsigned long long used; } nvmlMemory_v2_t;
enum { nvmlMemory_v2_internal = (unsigned int)(sizeof(nvmlMemory_v2_t) | (2 << 24U)) };
typedef struct { unsigned long long total; unsigned long long free; unsigned long long used; } nvmlMemory_t;
typedef enum { NVML_CLOCK_GRAPHICS = 0, NVML_CLOCK_MEM = 2 } nvmlClockType_t;
typedef struct { unsigned int gpu; unsigned int memory; } nvmlUtilization_t;

typedef nvmlReturn_t (*PFN_nvmlInit_v2)(void);
typedef nvmlReturn_t (*PFN_nvmlShutdown)(void);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetCount_v2)(unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetHandleByIndex_v2)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetName)(nvmlDevice_t, char*, unsigned int);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMaxClockInfo)(nvmlDevice_t, nvmlClockType_t, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetTemperature)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);

static struct {
    HMODULE dll;
    PFN_nvmlInit_v2 pfnInit;
    PFN_nvmlShutdown pfnShutdown;
    PFN_nvmlDeviceGetCount_v2 pfnGetCount;
    PFN_nvmlDeviceGetHandleByIndex_v2 pfnGetHandle;
    PFN_nvmlDeviceGetName pfnGetName;
    PFN_nvmlDeviceGetMaxClockInfo pfnGetClock;
    PFN_nvmlDeviceGetUtilizationRates pfnGetUtil;
    PFN_nvmlDeviceGetMemoryInfo pfnGetMem;
    PFN_nvmlDeviceGetTemperature pfnGetTemp;
    PFN_nvmlDeviceGetPowerUsage pfnGetPower;
} nv = {0};

static int load_nvml(void)
{
    if (nv.dll) return 1;
    nv.dll = LoadLibraryA("nvml.dll");
    if (!nv.dll) return 0;
    nv.pfnInit     = (PFN_nvmlInit_v2)GetProcAddress(nv.dll, "nvmlInit_v2");
    nv.pfnShutdown = (PFN_nvmlShutdown)GetProcAddress(nv.dll, "nvmlShutdown");
    nv.pfnGetCount = (PFN_nvmlDeviceGetCount_v2)GetProcAddress(nv.dll, "nvmlDeviceGetCount_v2");
    nv.pfnGetHandle= (PFN_nvmlDeviceGetHandleByIndex_v2)GetProcAddress(nv.dll, "nvmlDeviceGetHandleByIndex_v2");
    nv.pfnGetName  = (PFN_nvmlDeviceGetName)GetProcAddress(nv.dll, "nvmlDeviceGetName");
    nv.pfnGetClock = (PFN_nvmlDeviceGetMaxClockInfo)GetProcAddress(nv.dll, "nvmlDeviceGetMaxClockInfo");
    nv.pfnGetUtil  = (PFN_nvmlDeviceGetUtilizationRates)GetProcAddress(nv.dll, "nvmlDeviceGetUtilizationRates");
    nv.pfnGetMem   = (PFN_nvmlDeviceGetMemoryInfo)GetProcAddress(nv.dll, "nvmlDeviceGetMemoryInfo");
    nv.pfnGetTemp  = (PFN_nvmlDeviceGetTemperature)GetProcAddress(nv.dll, "nvmlDeviceGetTemperature");
    nv.pfnGetPower = (PFN_nvmlDeviceGetPowerUsage)GetProcAddress(nv.dll, "nvmlDeviceGetPowerUsage");
    if (!nv.pfnInit || !nv.pfnShutdown || !nv.pfnGetCount ||
        !nv.pfnGetHandle || !nv.pfnGetName) {
        FreeLibrary(nv.dll); nv.dll = NULL;
        return 0;
    }
    return 1;
}

static void unload_nvml(void)
{
    if (nv.dll) { FreeLibrary(nv.dll); nv.dll = NULL; memset(&nv, 0, sizeof(nv)); }
}

// --- WMI GPU enumeration ---
static BSTR mb(const wchar_t* s) { return SysAllocString(s); }

struct GpuEntry {
    wchar_t name[256];
    unsigned long long vramBytes;
    int isNvidia;
};

static int enum_gpus(GpuEntry* entries, int maxEntries)
{
    int count = 0;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool comInited = SUCCEEDED(hr);
    if (!comInited && hr != RPC_E_CHANGED_MODE) return 0;

    IWbemLocator* pLoc = NULL;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (void**)&pLoc)))
        { if (comInited) CoUninitialize(); return 0; }

    IWbemServices* pSvc = NULL;
    BSTR ns = mb(L"ROOT\\CIMV2");
    hr = pLoc->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    SysFreeString(ns);
    if (FAILED(hr)) { pLoc->Release(); if (comInited) CoUninitialize(); return 0; }

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    BSTR wql = mb(L"WQL");
    BSTR q = mb(L"SELECT Name, AdapterRAM, PNPDeviceID FROM Win32_VideoController");
    IEnumWbemClassObject* pEnum = NULL;
    hr = pSvc->ExecQuery(wql, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum);
    SysFreeString(wql); SysFreeString(q);

    if (SUCCEEDED(hr) && pEnum) {
        IWbemClassObject* pObj = NULL;
        ULONG ret = 0;
        while (count < maxEntries && pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK && ret > 0) {
            GpuEntry* e = &entries[count];
            e->name[0] = L'\0'; e->vramBytes = 0; e->isNvidia = 0;

            VARIANT vt;
            VariantInit(&vt);
            if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, NULL, NULL)) && vt.vt == VT_BSTR)
                wcsncpy_s(e->name, 256, vt.bstrVal, _TRUNCATE);
            VariantClear(&vt);

            VariantInit(&vt);
            if (SUCCEEDED(pObj->Get(L"AdapterRAM", 0, &vt, NULL, NULL))) {
                if (vt.vt == VT_UI4) e->vramBytes = vt.uiVal;
                else if (vt.vt == VT_UI8) e->vramBytes = vt.ullVal;
            }
            VariantClear(&vt);

            VARIANT vtId;
            VariantInit(&vtId);
            if (SUCCEEDED(pObj->Get(L"PNPDeviceID", 0, &vtId, NULL, NULL)) && vtId.vt == VT_BSTR) {
                if (wcsstr(vtId.bstrVal, L"VEN_10DE"))
                    e->isNvidia = 1;
            }
            VariantClear(&vtId);

            if (e->name[0] && wcsstr(e->name, L"NVIDIA"))
                e->isNvidia = 1;

            pObj->Release();
            count++;
        }
        pEnum->Release();
    }

    pSvc->Release(); pLoc->Release();
    if (comInited) CoUninitialize();
    return count;
}

static int has_substr(const char* s, const char* sub)
{
    return strstr(s, sub) != NULL;
}

void print_gpu_info(void)
{
    GpuEntry entries[8];
    int gpuCount = enum_gpus(entries, 8);
    if (gpuCount == 0) {
        print_block("GPU", "[No GPU detected]");
        return;
    }

    int nvmlOk = load_nvml();

    unsigned int nvCount = 0;
    char nvNames[8][256];
    int nvmlInited = 0;
    if (nvmlOk && nv.pfnInit() == NVML_SUCCESS) {
        nvmlInited = 1;
        if (nv.pfnGetCount(&nvCount) == NVML_SUCCESS && nvCount > 8) nvCount = 8;
        for (unsigned int ni = 0; ni < nvCount; ni++) {
            nvNames[ni][0] = '\0';
            nvmlDevice_t dev;
            if (nv.pfnGetHandle(ni, &dev) == NVML_SUCCESS && nv.pfnGetName)
                nv.pfnGetName(dev, nvNames[ni], sizeof(nvNames[ni]));
        }
    }

    for (int i = 0; i < gpuCount; i++) {
        GpuEntry* e = &entries[i];
        char nameBuf[256] = {0};
        WideCharToMultiByte(CP_UTF8, 0, e->name, -1, nameBuf, sizeof(nameBuf), NULL, NULL);

        int isIntegrated = 0;
        if (!e->isNvidia) {
            if (has_substr(nameBuf, "Radeon(TM) Graphics") ||
                has_substr(nameBuf, "Intel(R)") ||
                has_substr(nameBuf, "UHD Graphics") ||
                has_substr(nameBuf, "Iris"))
                isIntegrated = 1;
        }

        double vramGiB = e->vramBytes / (1024.0 * 1024.0 * 1024.0);
        double vramMiB = e->vramBytes / (1024.0 * 1024.0);

        char label[32];
        snprintf(label, sizeof(label), "GPU %d", i + 1);

        // Build main line
        char mainLine[512];
        int pos = 0;
        pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, "%s", nameBuf);

        // Match WMI GPU to NVML by name substring
        int nvIdx = -1;
        if (e->isNvidia && nvmlInited) {
            for (unsigned int ni = 0; ni < nvCount; ni++) {
                if (nvNames[ni][0] && has_substr(nvNames[ni], nameBuf + strlen(nameBuf) - 8))
                    { nvIdx = (int)ni; break; }
                if (nvNames[ni][0] && has_substr(nameBuf, nvNames[ni]))
                    { nvIdx = (int)ni; break; }
            }
            if (nvIdx < 0) {
                int nvidiaBefore = 0;
                for (int j = 0; j < i; j++) { if (entries[j].isNvidia) nvidiaBefore++; }
                if ((unsigned int)nvidiaBefore < nvCount) nvIdx = nvidiaBefore;
            }
        }

        unsigned int coreClock = 0;
        if (nvIdx >= 0 && nvmlInited) {
            nvmlDevice_t dev;
            if (nv.pfnGetHandle((unsigned int)nvIdx, &dev) == NVML_SUCCESS && nv.pfnGetClock)
                nv.pfnGetClock(dev, NVML_CLOCK_GRAPHICS, &coreClock);
        }

        if (coreClock > 0)
            pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " @ %.2f GHz", coreClock / 1000.0);

        if (vramGiB >= 1.0)
            pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " (%.2f GiB)", vramGiB);
        else if (vramMiB > 0)
            pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " (%.2f MiB)", vramMiB);

        pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " [%s]", isIntegrated ? "Integrated" : "Discrete");

        print_block_green(label, mainLine);

        // Detail line for NVIDIA: Core | Mem | Load | VRAM | Temp | Power
        if (nvIdx >= 0 && nvmlInited) {
            nvmlDevice_t dev;
            if (nv.pfnGetHandle((unsigned int)nvIdx, &dev) == NVML_SUCCESS) {
                unsigned int memClock = 0, util = 0, temp = 0, powerMw = 0;
                double vramUsed = 0, vramTotal = 0;

                if (nv.pfnGetClock) nv.pfnGetClock(dev, NVML_CLOCK_MEM, &memClock);
                if (nv.pfnGetUtil) { nvmlUtilization_t u; if (nv.pfnGetUtil(dev, &u) == NVML_SUCCESS) util = u.gpu; }
                if (nv.pfnGetMem) { nvmlMemory_t m; if (nv.pfnGetMem(dev, &m) == NVML_SUCCESS) { vramUsed = m.used / (1024.0*1024.0*1024.0); vramTotal = m.total / (1024.0*1024.0*1024.0); } }
                if (nv.pfnGetTemp) nv.pfnGetTemp(dev, NVML_TEMPERATURE_GPU, &temp);
                if (nv.pfnGetPower) { nv.pfnGetPower(dev, &powerMw); }

                char detail[512];
                snprintf(detail, sizeof(detail),
                    "Core %u MHz  |  Mem %u MHz  |  Load %u%%  |  VRAM %.1f / %.1f GiB  |  Temp %u\xc2\xb0""C  |  Power %u W",
                    coreClock, memClock, util, vramUsed, vramTotal, temp, powerMw / 1000);
                print_detail(detail);
            }
        }
    }

    if (nvmlInited) nv.pfnShutdown();
    unload_nvml();
}
