#include "memory_spd.h"
#include "debug.h"

#include <cstdio>
#include <cstring>
#include <windows.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

BSTR mbstr(const wchar_t* s) { return SysAllocString(s); }

/* Win32_PhysicalMemory.SMBIOSMemoryType -> short label. */
const char* mem_type_name(unsigned int t)
{
    switch (t) {
        case 20: return "DDR";
        case 21: return "DDR2";
        case 24: return "DDR3";
        case 26: return "DDR4";
        case 34: return "DDR5";
        case 35: return "LPDDR4";
        case 36: return "LPDDR5";
        default: return 0;
    }
}

/* uint64 and uint8 values arrive as decimal strings from some providers. */
unsigned long long bstr_to_u64(BSTR b)
{
    unsigned long long out = 0;
    if (!b) return 0;
    while (*b) {
        if (*b < '0' || *b > '9') break;
        out = out * 10 + (unsigned long long)(*b - '0');
        b++;
    }
    return out;
}

long as_long(IWbemClassObject* obj, const wchar_t* prop)
{
    VARIANT vt;
    long out = -1;
    VariantInit(&vt);
    BSTR p = mbstr(prop);
    if (SUCCEEDED(obj->Get(p, 0, &vt, NULL, NULL))) {
        if (vt.vt == VT_I4) out = vt.lVal;
        else if (vt.vt == VT_UI4) out = (long)vt.uiVal;
        else if (vt.vt == VT_I2) out = (long)*(short*)&vt.cVal;
        else if (vt.vt == VT_UI2) out = (long)*(unsigned short*)&vt.cVal;
        else if (vt.vt == VT_UI1) out = (long)vt.bVal;
        else if (vt.vt == VT_I1) out = (long)vt.cVal;
        /* uint64 and uint8 arrive as decimal strings from some providers. */
        else if (vt.vt == VT_BSTR && vt.bstrVal) out = (long)bstr_to_u64(vt.bstrVal);
    }
    SysFreeString(p);
    VariantClear(&vt);
    return out;
}

unsigned long long as_u64(IWbemClassObject* obj, const wchar_t* prop)
{
    VARIANT vt;
    unsigned long long out = 0;
    VariantInit(&vt);
    BSTR p = mbstr(prop);
    if (SUCCEEDED(obj->Get(p, 0, &vt, NULL, NULL))) {
        if (vt.vt == VT_UI8) out = (unsigned long long)vt.ullVal;
        else if (vt.vt == VT_I8) out = (unsigned long long)vt.llVal;
        else if (vt.vt == VT_UI4) out = vt.uiVal;
        else if (vt.vt == VT_I4) out = (unsigned long long)(long)vt.lVal;
        else if (vt.vt == VT_BSTR && vt.bstrVal) out = bstr_to_u64(vt.bstrVal);
    }
    SysFreeString(p);
    VariantClear(&vt);
    return out;
}

} /* namespace */

void memory_collect_spd(hf_state* s)
{
    HRESULT hr;
    bool comInited;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnum = NULL;
    unsigned int moduleMb = 0;

    debug_begin("ram-spd");

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    comInited = SUCCEEDED(hr);
    if (!comInited && hr != RPC_E_CHANGED_MODE) {
        debug_log("CoInitializeEx failed: %s", debug_hresult(hr));
        return;
    }

    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (void**)&pLoc);
    if (FAILED(hr)) {
        debug_log("CoCreateInstance(WbemLocator) failed: %s", debug_hresult(hr));
        if (comInited) CoUninitialize();
        return;
    }

    BSTR ns = mbstr(L"ROOT\\CIMV2");
    hr = pLoc->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    SysFreeString(ns);
    if (FAILED(hr)) {
        debug_log("ConnectServer failed: %s", debug_hresult(hr));
        pLoc->Release();
        if (comInited) CoUninitialize();
        return;
    }

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    BSTR wql = mbstr(L"WQL");
    /* SELECT * rather than a projection: naming ConfiguredCASLatency in the
       projection is rejected with WBEM_E_INVALID_QUERY on some builds. */
    BSTR query = mbstr(L"SELECT * FROM Win32_PhysicalMemory");
    hr = pSvc->ExecQuery(wql, query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum);
    SysFreeString(wql);
    SysFreeString(query);
    debug_log("ExecQuery returned %s, enum=%p", debug_hresult(hr), (void*)pEnum);
    if (FAILED(hr)) {
        debug_log("ExecQuery(Win32_PhysicalMemory) failed: %s", debug_hresult(hr));
        pSvc->Release();
        pLoc->Release();
        if (comInited) CoUninitialize();
        return;
    }

    IWbemClassObject* pObj = NULL;
    ULONG returned = 0;
    int rows = 0;
    HRESULT nextHr;
    while ((nextHr = pEnum->Next(1500, 1, &pObj, &returned)) == S_OK && returned > 0) {
        rows++;
        long speed = as_long(pObj, L"ConfiguredClockSpeed");
        long cas = as_long(pObj, L"ConfiguredCASLatency");
        long type = as_long(pObj, L"SMBIOSMemoryType");
        unsigned long long cap = as_u64(pObj, L"Capacity");

        if (speed > 0) s->ram_mhz = (unsigned int)speed;
        if (cas > 0) s->ram_cl = (unsigned int)cas;
        if (cap > 0 && cap > moduleMb) moduleMb = (unsigned int)(cap / (1024ULL * 1024ULL));
        if (type > 0) {
            const char* name = mem_type_name((unsigned int)type);
            if (name) snprintf(s->ram_type, sizeof(s->ram_type), "%s", name);
        }
        s->ram_modules++;
        debug_log("module %u: speed=%ld CAS=%ld type=%ld capacity=%llu",
                  s->ram_modules, speed, cas, type, cap);
        pObj->Release();
    }

    s->ram_module_bytes = (unsigned long long)moduleMb * 1024ULL * 1024ULL;
    debug_log("enumeration finished after %d rows, last Next returned %s",
              rows, debug_hresult(nextHr));

    pEnum->Release();
    pSvc->Release();
    pLoc->Release();
    if (comInited) CoUninitialize();

    debug_log("summary: %u MHz CL%u type='%s' modules=%u module=%llu bytes",
              s->ram_mhz, s->ram_cl, s->ram_type, s->ram_modules, s->ram_module_bytes);
    debug_end();
}