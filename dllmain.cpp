// dllmain.cpp : DLL entry points and COM registration

#include "pch.h"
#include "ComicTooltipExt.h"
#include "ComicPreviewHandler.h"

// Tooltip handler shell extension GUID
// {00021500-0000-0000-C000-000000000046}
static const wchar_t k_szTooltipHandlerGuid[] = L"{00021500-0000-0000-C000-000000000046}";
static const wchar_t k_szCLSID[]              = L"{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}";
static const wchar_t k_szInprocServer[]       = L"CLSID\\{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}\\InprocServer32";

// Preview handler shell extension GUID
// {8895b1c6-b41f-4c1c-a562-0d564250836f}
static const wchar_t k_szPreviewHandlerGuid[] = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";
static const wchar_t k_szPreviewCLSID[]       = L"{B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}";
static const wchar_t k_szPreviewInproc[]      = L"CLSID\\{B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}\\InprocServer32";

class CComicTooltipExtModule : public ATL::CAtlDllModuleT<CComicTooltipExtModule>
{
};

CComicTooltipExtModule _AtlModule;

OBJECT_ENTRY_AUTO(CLSID_ComicTooltipExt, CComicTooltipExt)
OBJECT_ENTRY_AUTO(CLSID_ComicPreviewHandler, CComicPreviewHandler)

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hInstance);
    return _AtlModule.DllMain(dwReason, lpReserved);
}

STDAPI DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

// -----------------------------------------------------------------------
// Registration helpers
// -----------------------------------------------------------------------

static HRESULT RegisterShellexHandler(LPCWSTR pszExt, LPCWSTR pszHandlerGuid, LPCWSTR pszClsid, bool bUnregister)
{
    wchar_t szKey[MAX_PATH];
    swprintf_s(szKey, MAX_PATH, L"%s\\shellex\\%s", pszExt, pszHandlerGuid);

    if (bUnregister)
    {
        RegDeleteKey(HKEY_CLASSES_ROOT, szKey);
        return S_OK;
    }

    HKEY hKey = NULL;
    LONG lr = RegCreateKeyEx(HKEY_CLASSES_ROOT, szKey, 0, NULL,
                             REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lr != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lr);

    lr = RegSetValueEx(hKey, NULL, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(pszClsid),
                       static_cast<DWORD>((wcslen(pszClsid) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lr);

    return S_OK;
}

static HRESULT RegisterCLSID(LPCWSTR pszInprocKey, LPCWSTR pszClsid, LPCWSTR pszModulePath, bool bUnregister)
{
    if (bUnregister)
    {
        RegDeleteKey(HKEY_CLASSES_ROOT, pszInprocKey);
        wchar_t szClsidKey[MAX_PATH];
        swprintf_s(szClsidKey, MAX_PATH, L"CLSID\\%s", pszClsid);
        RegDeleteKey(HKEY_CLASSES_ROOT, szClsidKey);
        return S_OK;
    }

    HKEY hKey = NULL;
    LONG lr = RegCreateKeyEx(HKEY_CLASSES_ROOT, pszInprocKey, 0, NULL,
                             REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lr != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lr);

    lr = RegSetValueEx(hKey, NULL, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(pszModulePath),
                       static_cast<DWORD>((wcslen(pszModulePath) + 1) * sizeof(wchar_t)));

    static const wchar_t k_szApartment[] = L"Apartment";
    if (lr == ERROR_SUCCESS)
        lr = RegSetValueEx(hKey, L"ThreadingModel", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(k_szApartment),
                           static_cast<DWORD>((wcslen(k_szApartment) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lr);

    return S_OK;
}

STDAPI DllRegisterServer(void)
{
    wchar_t szModule[MAX_PATH] = {};
    GetModuleFileName(ATL::_AtlBaseModule.GetModuleInstance(), szModule, MAX_PATH);

    // Register tooltip handler CLSID
    HRESULT hr = RegisterCLSID(k_szInprocServer, k_szCLSID, szModule, false);
    if (SUCCEEDED(hr)) hr = RegisterShellexHandler(L".cbr", k_szTooltipHandlerGuid, k_szCLSID, false);
    if (SUCCEEDED(hr)) hr = RegisterShellexHandler(L".cbz", k_szTooltipHandlerGuid, k_szCLSID, false);

    // Register preview handler CLSID
    if (SUCCEEDED(hr)) hr = RegisterCLSID(k_szPreviewInproc, k_szPreviewCLSID, szModule, false);
    if (SUCCEEDED(hr)) hr = RegisterShellexHandler(L".cbr", k_szPreviewHandlerGuid, k_szPreviewCLSID, false);
    if (SUCCEEDED(hr)) hr = RegisterShellexHandler(L".cbz", k_szPreviewHandlerGuid, k_szPreviewCLSID, false);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return hr;
}

STDAPI DllUnregisterServer(void)
{
    // Unregister tooltip handler
    RegisterShellexHandler(L".cbr", k_szTooltipHandlerGuid, k_szCLSID, true);
    RegisterShellexHandler(L".cbz", k_szTooltipHandlerGuid, k_szCLSID, true);
    RegisterCLSID(k_szInprocServer, k_szCLSID, NULL, true);

    // Unregister preview handler
    RegisterShellexHandler(L".cbr", k_szPreviewHandlerGuid, k_szPreviewCLSID, true);
    RegisterShellexHandler(L".cbz", k_szPreviewHandlerGuid, k_szPreviewCLSID, true);
    RegisterCLSID(k_szPreviewInproc, k_szPreviewCLSID, NULL, true);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}
