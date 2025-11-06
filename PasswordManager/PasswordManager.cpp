#define SODIUM_STATIC

#include <sodium.h>
#include <iostream>
#include <string>

#include <windows.h>
#include <comdef.h>
#include <Wbemidl.h>


class Screen
{

};

class App
{

};

#pragma comment(lib, "wbemuuid.lib")

int main()
{
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;

    hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(hr)) return 1;

    IWbemLocator* locator = NULL;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&locator);
    if (FAILED(hr)) return 1;

    IWbemServices* svc = NULL;
    hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &svc);
    if (FAILED(hr)) return 1;

    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);

    IEnumWbemClassObject* enumerator = NULL;
    hr = svc->ExecQuery(bstr_t("WQL"),
        bstr_t("SELECT DeviceID, Model, SerialNumber FROM Win32_DiskDrive"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &enumerator);

    IWbemClassObject* obj;
    ULONG retVal = 0;
    while (enumerator) {
        HRESULT hr2 = enumerator->Next(WBEM_INFINITE, 1, &obj, &retVal);
        if (0 == retVal) break;

        VARIANT val;
        obj->Get(L"DeviceID", 0, &val, 0, 0);
        std::wcout << L"DeviceID: " << val.bstrVal << L"\n";
        VariantClear(&val);

        obj->Get(L"Model", 0, &val, 0, 0);
        std::wcout << L"Model: " << val.bstrVal << L"\n";
        VariantClear(&val);

        obj->Get(L"SerialNumber", 0, &val, 0, 0);
        std::wcout << L"Serial: " << val.bstrVal << "\n\n";
        VariantClear(&val);

        obj->Release();
    }

    svc->Release();
    locator->Release();
    enumerator->Release();
    CoUninitialize();

    return 0;
}
