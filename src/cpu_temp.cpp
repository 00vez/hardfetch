#include "cpu_temp.h"
#include "debug.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <windows.h>

/* The SDK ships no usable perflib header, so the documented structures and
   the five ntdll entry points are declared here. */
typedef struct _PERF_INSTANCE_HEADER {
    ULONG Size;
    ULONG InstanceId;
} PERF_INSTANCE_HEADER, *PPERF_INSTANCE_HEADER;

typedef struct _PERF_COUNTER_IDENTIFIER {
    GUID  CounterSetGuid;
    ULONG Status;
    ULONG Size;
    ULONG CounterId;
    ULONG InstanceId;
    ULONG Index;
    ULONG Reserved;
} PERF_COUNTER_IDENTIFIER, *PPERF_COUNTER_IDENTIFIER;

typedef struct _PERF_DATA_HEADER {
    ULONG       dwTotalSize;
    ULONG       dwNumCounters;
    LONGLONG    PerfTimeStamp;
    LONGLONG    PerfTime100NSec;
    LONGLONG    PerfFreq;
    SYSTEMTIME  SystemTime;
} PERF_DATA_HEADER, *PPERF_DATA_HEADER;

enum PerfCounterDataType {
    PERF_ERROR_RETURN = 0,
    PERF_SINGLE_COUNTER = 1,
    PERF_MULTIPLE_COUNTERS = 2,
    PERF_MULTIPLE_INSTANCES = 4,
    PERF_COUNTERSET = 6
};

typedef struct _PERF_COUNTER_HEADER {
    ULONG               dwStatus;
    PerfCounterDataType dwType;
    ULONG               dwSize;
    ULONG               Reserved;
} PERF_COUNTER_HEADER, *PPERF_COUNTER_HEADER;

typedef struct _PERF_MULTI_COUNTERS {
    ULONG dwSize;
    ULONG dwCounters;
} PERF_MULTI_COUNTERS, *PPERF_MULTI_COUNTERS;

typedef struct _PERF_COUNTER_DATA {
    ULONG dwDataSize;
    ULONG dwSize;
} PERF_COUNTER_DATA, *PPERF_COUNTER_DATA;

#define PERF_WILDCARD_COUNTER 0xFFFFFFFFUL

typedef ULONG(WINAPI* PFN_PerfOpenQueryHandle)(LPCWSTR, HANDLE*);
typedef ULONG(WINAPI* PFN_PerfAddCounters)(HANDLE, PPERF_COUNTER_IDENTIFIER, DWORD);
typedef ULONG(WINAPI* PFN_PerfCloseQueryHandle)(HANDLE);
typedef ULONG(WINAPI* PFN_PerfQueryCounterData)(HANDLE, PPERF_DATA_HEADER, DWORD, DWORD*);
typedef ULONG(WINAPI* PFN_PerfEnumerateCounterSetInstances)(
    LPCWSTR, LPCGUID, PPERF_INSTANCE_HEADER, DWORD, DWORD*);

/* The provider writes the instance name directly behind the identifier, so the
   identifier Size has to span the whole structure. */
struct ThermalQuerySpec {
    PERF_COUNTER_IDENTIFIER Identifier;
    WCHAR Name[16];
};

static const GUID kThermalZoneGuid = {
    0x52bc5412, 0xdac2, 0x449c, { 0x8b, 0xc2, 0x96, 0x44, 0x38, 0x88, 0xfe, 0x6b }
};

static const size_t kNameCount = sizeof(((ThermalQuerySpec*)0)->Name) / sizeof(WCHAR);

static bool load_perflib(PFN_PerfOpenQueryHandle* pOpen, PFN_PerfAddCounters* pAdd,
                         PFN_PerfCloseQueryHandle* pClose, PFN_PerfQueryCounterData* pQuery,
                         PFN_PerfEnumerateCounterSetInstances* pEnum)
{
    /* The provider API is exported by advapi32.dll on current Windows builds;
       older documentation and other tools point at ntdll, which does not
       export these names. Try both. */
    static const char* modules[] = { "advapi32.dll", "ntdll.dll" };
    size_t m;

    ZeroMemory(pOpen, sizeof(*pOpen));
    ZeroMemory(pAdd, sizeof(*pAdd));
    ZeroMemory(pClose, sizeof(*pClose));
    ZeroMemory(pQuery, sizeof(*pQuery));
    ZeroMemory(pEnum, sizeof(*pEnum));

    for (m = 0; m < sizeof(modules) / sizeof(modules[0]); m++) {
        HMODULE h = GetModuleHandleA(modules[m]);
        if (!h) { debug_log("%s not loaded", modules[m]); continue; }

        *pOpen = (PFN_PerfOpenQueryHandle)GetProcAddress(h, "PerfOpenQueryHandle");
        *pAdd = (PFN_PerfAddCounters)GetProcAddress(h, "PerfAddCounters");
        *pClose = (PFN_PerfCloseQueryHandle)GetProcAddress(h, "PerfCloseQueryHandle");
        *pQuery = (PFN_PerfQueryCounterData)GetProcAddress(h, "PerfQueryCounterData");
        *pEnum = (PFN_PerfEnumerateCounterSetInstances)GetProcAddress(h, "PerfEnumerateCounterSetInstances");
        debug_log("%s: open=%p add=%p close=%p query=%p enum=%p",
                  modules[m], (void*)*pOpen, (void*)*pAdd, (void*)*pClose,
                  (void*)*pQuery, (void*)*pEnum);

        if (*pOpen && *pAdd && *pClose && *pQuery && *pEnum) return true;
    }
    return false;
}

static int read_temperature(void)
{
    PFN_PerfOpenQueryHandle pOpen;
    PFN_PerfAddCounters pAdd;
    PFN_PerfCloseQueryHandle pClose;
    PFN_PerfQueryCounterData pQuery;
    PFN_PerfEnumerateCounterSetInstances pEnum;
    HANDLE hQuery = NULL;
    DWORD status = 0, dataSize = 0;
    ThermalQuerySpec spec;
    int result = -1;

    if (!load_perflib(&pOpen, &pAdd, &pClose, &pQuery, &pEnum)) {
        debug_log("perflib exports missing in ntdll");
        return -1;
    }

    ZeroMemory(&spec, sizeof(spec));
    spec.Identifier.CounterSetGuid = kThermalZoneGuid;
    spec.Identifier.Size = sizeof(spec);
    spec.Identifier.CounterId = PERF_WILDCARD_COUNTER;
    spec.Identifier.InstanceId = PERF_WILDCARD_COUNTER;
    wcscpy_s(spec.Name, kNameCount, L"\\_TZ.CPUZ");

    status = pEnum(NULL, &kThermalZoneGuid, NULL, 0, &dataSize);
    if (status != ERROR_NOT_ENOUGH_MEMORY) {
        debug_log("PerfEnumerateCounterSetInstances probe returned 0x%08lX, no thermal zones",
                  (unsigned long)status);
        return -1;
    }
    if (dataSize <= sizeof(PERF_INSTANCE_HEADER)) {
        debug_log("thermal zone provider present but empty (%lu bytes)",
                  (unsigned long)dataSize);
        return -1;
    }

    {
        std::vector<unsigned char> headers(dataSize);
        BYTE* p;
        DWORD left = dataSize;
        bool found = false;

        status = pEnum(NULL, &kThermalZoneGuid, (PPERF_INSTANCE_HEADER)headers.data(),
                       dataSize, &dataSize);
        if (status != ERROR_SUCCESS) {
            debug_log("PerfEnumerateCounterSetInstances(headers) 0x%08lX",
                      (unsigned long)status);
            return -1;
        }

        p = headers.data();
        while (left > sizeof(PERF_INSTANCE_HEADER)) {
            const PERF_INSTANCE_HEADER* ih = (const PERF_INSTANCE_HEADER*)p;
            const WCHAR* name = (const WCHAR*)(p + sizeof(PERF_INSTANCE_HEADER));
            if (wcscmp(name, spec.Name) == 0) { found = true; break; }
            if (ih->Size == 0 || ih->Size > left) break;
            left -= ih->Size;
            p += ih->Size;
        }

        if (!found) {
            const WCHAR* first = (const WCHAR*)(headers.data() + sizeof(PERF_INSTANCE_HEADER));
            debug_log("instance _TZ.CPUZ absent, falling back to '%ls'", first);
            wcscpy_s(spec.Name, kNameCount, first);
        } else {
            debug_log("instance _TZ.CPUZ present");
        }
    }

    status = pOpen(NULL, &hQuery);
    if (status != ERROR_SUCCESS) {
        debug_log("PerfOpenQueryHandle 0x%08lX", (unsigned long)status);
        return -1;
    }

    status = pAdd(hQuery, &spec.Identifier, sizeof(spec));
    if (status != ERROR_SUCCESS || spec.Identifier.Status != ERROR_SUCCESS) {
        debug_log("PerfAddCounters 0x%08lX (identifier status 0x%08lX)",
                  (unsigned long)status, (unsigned long)spec.Identifier.Status);
        pClose(hQuery);
        return -1;
    }

    status = pQuery(hQuery, NULL, 0, &dataSize);
    if (status != ERROR_NOT_ENOUGH_MEMORY || dataSize <= sizeof(PERF_DATA_HEADER)) {
        debug_log("PerfQueryCounterData size probe 0x%08lX size=%lu",
                  (unsigned long)status, (unsigned long)dataSize);
        pClose(hQuery);
        return -1;
    }

    {
        std::vector<unsigned char> data(dataSize);
        PERF_COUNTER_HEADER* counterHeader;
        PERF_MULTI_COUNTERS* multi;
        ULONG* ids;
        PERF_COUNTER_DATA* cd;
        int plainTemp = -1, preciseTemp = -1;
        ULONG i;

        status = pQuery(hQuery, (PPERF_DATA_HEADER)data.data(), dataSize, &dataSize);
        if (status != ERROR_SUCCESS) {
            debug_log("PerfQueryCounterData 0x%08lX", (unsigned long)status);
            pClose(hQuery);
            return -1;
        }

        counterHeader = (PERF_COUNTER_HEADER*)(data.data() + sizeof(PERF_DATA_HEADER));
        if (counterHeader->dwType != PERF_MULTIPLE_COUNTERS) {
            debug_log("unexpected result type %d (status 0x%08lX)",
                      (int)counterHeader->dwType, (unsigned long)counterHeader->dwStatus);
            pClose(hQuery);
            return -1;
        }

        multi = (PERF_MULTI_COUNTERS*)(counterHeader + 1);
        ids = (ULONG*)(multi + 1);
        cd = (PERF_COUNTER_DATA*)((BYTE*)multi + multi->dwSize);

        for (i = 0; i < multi->dwCounters; i++) {
            if (cd->dwDataSize == sizeof(int32_t)) {
                int32_t value = *(int32_t*)((BYTE*)cd + sizeof(PERF_COUNTER_DATA));
                if (value != 0) {
                    if (ids[i] == 0 && value > 1000) plainTemp = value - 273;
                    else if (ids[i] == 3) preciseTemp = (int)(value / 10.0 - 273.0 + 0.5);
                }
            }
            if (cd->dwSize == 0) break;
            cd = (PERF_COUNTER_DATA*)((BYTE*)cd + cd->dwSize);
        }

        if (preciseTemp > -100) result = preciseTemp;
        else if (plainTemp > -100) result = plainTemp;

        debug_log("counters=%lu plain=%d precise=%d -> %d",
                  (unsigned long)multi->dwCounters, plainTemp, preciseTemp, result);
    }

    pClose(hQuery);
    return result;
}

void cpu_temp_collect(hf_state* s)
{
    int c;

    debug_begin("temp");
    c = read_temperature();
    s->cpu_temp_c = c;
    debug_log("cpu temp = %d C", c);
    debug_end();
}