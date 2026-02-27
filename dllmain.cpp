// dllmain.cpp : DLL entry points and COM registration

#include "pch.h"
#include "ComicTooltipExt.h"

// Tooltip handler shell extension GUID
// {00021500-0000-0000-C000-000000000046}
static const wchar_t k_szTooltipHandlerGuid[] = L"{00021500-0000-0000-C000-000000000046}";
static const wchar_t k_szCLSID[]              = L"{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}";
static const wchar_t k_szInprocServer[]       = L"CLSID\\{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}\\InprocServer32";

class CComicTooltipExtModule : public ATL::CAtlDllModuleT<CComicTooltipExtModule>
{
};

CComicTooltipExtModule _AtlModule;

OBJECT_ENTRY_AUTO(CLSID_ComicTooltipExt, CComicTooltipExt)

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

static HRESULT RegisterFileTypeHandler(LPCWSTR pszExt, bool bUnregister)
{
    wchar_t szKey[MAX_PATH];
    swprintf_s(szKey, MAX_PATH, L"%s\\shellex\\%s", pszExt, k_szTooltipHandlerGuid);

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
                       reinterpret_cast<const BYTE*>(k_szCLSID),
                       static_cast<DWORD>((wcslen(k_szCLSID) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lr);

    return S_OK;
}

static HRESULT RegisterCLSID(LPCWSTR pszModulePath, bool bUnregister)
{
    if (bUnregister)
    {
        RegDeleteKey(HKEY_CLASSES_ROOT, k_szInprocServer);
        wchar_t szClsidKey[MAX_PATH];
        swprintf_s(szClsidKey, MAX_PATH, L"CLSID\\%s", k_szCLSID);
        RegDeleteKey(HKEY_CLASSES_ROOT, szClsidKey);
        return S_OK;
    }

    HKEY hKey = NULL;
    LONG lr = RegCreateKeyEx(HKEY_CLASSES_ROOT, k_szInprocServer, 0, NULL,
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

    HRESULT hr = RegisterCLSID(szModule, false);
    if (SUCCEEDED(hr)) hr = RegisterFileTypeHandler(L".cbr", false);
    if (SUCCEEDED(hr)) hr = RegisterFileTypeHandler(L".cbz", false);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return hr;
}

STDAPI DllUnregisterServer(void)
{
    RegisterFileTypeHandler(L".cbr", true);
    RegisterFileTypeHandler(L".cbz", true);
    RegisterCLSID(NULL, true);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}
