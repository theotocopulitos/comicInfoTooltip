// TestPropHandler.cpp - Tests the ComicTooltipExt property handler directly
#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <stdio.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "propsys.lib")

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        wprintf(L"Usage: TestPropHandler <path-to-cbz-or-cbr>\n");
        return 1;
    }

    CoInitialize(NULL);

    const CLSID CLSID_ComicTooltipExt = {0xa1b2c3d4,0xe5f6,0x7890,{0xab,0xcd,0xef,0x12,0x34,0x56,0x78,0x92}};

    // Test 1: Create the COM object
    IPropertyStore* pStore = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ComicTooltipExt, NULL, CLSCTX_INPROC_SERVER,
                                   IID_IPropertyStore, (void**)&pStore);
    wprintf(L"CoCreateInstance: 0x%08X\n", hr);
    if (FAILED(hr)) { CoUninitialize(); return 1; }

    // Test 2: Initialize with the file path
    IInitializeWithFile* pInit = NULL;
    hr = pStore->QueryInterface(__uuidof(IInitializeWithFile), (void**)&pInit);
    wprintf(L"QI(IInitializeWithFile): 0x%08X\n", hr);
    if (SUCCEEDED(hr)) {
        hr = pInit->Initialize(argv[1], STGM_READ);
        wprintf(L"Initialize('%s'): 0x%08X\n", argv[1], hr);
        pInit->Release();
    }

    // Test 3: Get System.Comment property
    PROPVARIANT pv;
    PropVariantInit(&pv);
    hr = pStore->GetValue(PKEY_Comment, &pv);
    wprintf(L"GetValue(PKEY_Comment): 0x%08X\n", hr);
    if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR)
        wprintf(L"Tooltip text:\n---\n%s\n---\n", pv.pwszVal);
    else
        wprintf(L"PropVariant type: vt=%d\n", pv.vt);
    PropVariantClear(&pv);

    pStore->Release();

    // Test 4: Try via PSGetItemPropertyHandler (the way Explorer does it)
    wprintf(L"\n--- Via PSGetItemPropertyHandler ---\n");
    IPropertyStore* pStore2 = NULL;
    hr = PSGetItemPropertyHandler(NULL, FALSE, IID_IPropertyStore, (void**)&pStore2);
    wprintf(L"PSGetItemPropertyHandler: 0x%08X (expected E_INVALIDARG=0x80070057)\n", hr);

    CoUninitialize();
    return 0;
}
