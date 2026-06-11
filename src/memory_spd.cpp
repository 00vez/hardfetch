#include "memory_spd.h"

#include <windows.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static BSTR mbstr(const wchar_t* s) { return SysAllocString(s); }

int get_memory_speed(unsigned int* outSpeed, unsigned int* outCAS)
{
    *outCAS = 0;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool comInited = SUCCEEDED(hr);
    if (!comInited && hr != RPC_E_CHANGED_MODE)
        return -1;

    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (void**)&pLoc);
    if (FAILED(hr)) { if (comInited) CoUninitialize(); return -1; }

    IWbemServices* pSvc = NULL;
    hr = pLoc->ConnectServer(mbstr(L"ROOT\\CIMV2"), NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    if (FAILED(hr)) { pLoc->Release(); if (comInited) CoUninitialize(); return -1; }

    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hr)) { pSvc->Release(); pLoc->Release(); if (comInited) CoUninitialize(); return -1; }

    BSTR wql = mbstr(L"WQL");
    BSTR query = mbstr(L"SELECT ConfiguredClockSpeed FROM Win32_PhysicalMemory");
    IEnumWbemClassObject* pEnum = NULL;
    hr = pSvc->ExecQuery(wql, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum);
    SysFreeString(wql);
    SysFreeString(query);
    if (FAILED(hr)) { pSvc->Release(); pLoc->Release(); if (comInited) CoUninitialize(); return -1; }

    IWbemClassObject* pObj = NULL;
    ULONG returned = 0;
    hr = pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned);
    int result = -1;
    if (SUCCEEDED(hr) && returned > 0) {
        VARIANT vt;
        VariantInit(&vt);
        BSTR prop = mbstr(L"ConfiguredClockSpeed");
        hr = pObj->Get(prop, 0, &vt, NULL, NULL);
        SysFreeString(prop);
        if (SUCCEEDED(hr) && (vt.vt == VT_UI4 || vt.vt == VT_I4)) {
            *outSpeed = (vt.vt == VT_UI4) ? vt.uiVal : (unsigned int)vt.lVal;
            result = 0;
        }
        VariantClear(&vt);
        pObj->Release();
    }

    pEnum->Release();
    pSvc->Release();
    pLoc->Release();
    if (comInited) CoUninitialize();
    return result;
}
