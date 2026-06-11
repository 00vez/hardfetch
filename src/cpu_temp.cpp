#include "cpu_temp.h"
#include "output.h"

#include <cstdio>
#include <windows.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static BSTR make_bstr(const wchar_t* s)
{
    return SysAllocString(s);
}

static HRESULT wmi_query(const wchar_t* ns, const wchar_t* queryStr,
                         const wchar_t* prop, LONG* outVal)
{
    *outVal = -1;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool comInited = SUCCEEDED(hr);
    if (!comInited && hr != RPC_E_CHANGED_MODE)
        return hr;

    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (void**)&pLoc);
    if (FAILED(hr)) { if (comInited) CoUninitialize(); return hr; }

    IWbemServices* pSvc = NULL;
    BSTR bstrNs = make_bstr(ns);
    hr = pLoc->ConnectServer(bstrNs, NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    SysFreeString(bstrNs);
    if (FAILED(hr)) { pLoc->Release(); if (comInited) CoUninitialize(); return hr; }

    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hr)) { pSvc->Release(); pLoc->Release(); if (comInited) CoUninitialize(); return hr; }

    BSTR wql = make_bstr(L"WQL");
    BSTR bstrQuery = make_bstr(queryStr);
    IEnumWbemClassObject* pEnum = NULL;
    hr = pSvc->ExecQuery(wql, bstrQuery, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum);
    SysFreeString(wql);
    SysFreeString(bstrQuery);
    if (FAILED(hr)) { pSvc->Release(); pLoc->Release(); if (comInited) CoUninitialize(); return hr; }

    IWbemClassObject* pObj = NULL;
    ULONG returned = 0;
    hr = pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned);
    if (SUCCEEDED(hr) && returned > 0) {
        VARIANT vt;
        VariantInit(&vt);
        BSTR bstrProp = make_bstr(prop);
        hr = pObj->Get(bstrProp, 0, &vt, NULL, NULL);
        SysFreeString(bstrProp);
        if (SUCCEEDED(hr) && (vt.vt == VT_I4 || vt.vt == VT_UI4)) {
            *outVal = (vt.vt == VT_I4) ? vt.lVal : (LONG)vt.uiVal;
            hr = S_OK;
        } else {
            hr = E_FAIL;
        }
        VariantClear(&vt);
        pObj->Release();
    } else {
        hr = E_FAIL;
    }

    pEnum->Release();
    pSvc->Release();
    pLoc->Release();
    if (comInited) CoUninitialize();
    return hr;
}

static int get_cpu_temp_celsius(void)
{
    LONG temp = -1;
    if (wmi_query(L"ROOT\\WMI", L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature",
                  L"CurrentTemperature", &temp) == S_OK && temp >= 0)
        return (int)(temp / 10.0 - 273.15 + 0.5);
    if (wmi_query(L"ROOT\\CIMV2",
                  L"SELECT Temperature FROM Win32_PerfFormattedData_Counters_ThermalZoneInformation",
                  L"Temperature", &temp) == S_OK && temp >= 0)
        return (int)(temp / 10.0 - 273.15 + 0.5);
    return -1;
}

void print_cpu_temp_power(double load)
{
    char detail[256];
    int pos = snprintf(detail, sizeof(detail), "Load  %.0f%%", load);

    int tempC = get_cpu_temp_celsius();
    if (tempC >= 0)
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  %d\xc2\xb0""C", tempC);
    else
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  N/A");

    print_detail(detail);
}
