// ComicTooltipExt.h : Declaration of the CComicTooltipExt

#pragma once
#include "resource.h"
#include "ComicInfo.h"

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
EXTERN_C const CLSID CLSID_ComicTooltipExt;

class ATL_NO_VTABLE CComicTooltipExt :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CComicTooltipExt, &CLSID_ComicTooltipExt>,
    public IPersistFile,
    public IQueryInfo
{
public:
    CComicTooltipExt() {}

    DECLARE_NOT_AGGREGATABLE(CComicTooltipExt)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CComicTooltipExt)
        COM_INTERFACE_ENTRY(IPersistFile)
        COM_INTERFACE_ENTRY2(IPersist, IPersistFile)
        COM_INTERFACE_ENTRY(IQueryInfo)
    END_COM_MAP()

    // IPersist
    STDMETHOD(GetClassID)(CLSID* pClassID);

    // IPersistFile
    STDMETHOD(IsDirty)()                              { return S_FALSE; }
    STDMETHOD(Load)(LPCOLESTR pszFileName, DWORD dwMode);
    STDMETHOD(Save)(LPCOLESTR, BOOL)                  { return E_NOTIMPL; }
    STDMETHOD(SaveCompleted)(LPCOLESTR)               { return E_NOTIMPL; }
    STDMETHOD(GetCurFile)(LPOLESTR* ppszFileName);

    // IQueryInfo
    STDMETHOD(GetInfoFlags)(DWORD* pdwFlags);
    STDMETHOD(GetInfoTip)(DWORD dwFlags, LPWSTR* ppwszTipText);

private:
    CString m_strFileName;

    CString GenerateTooltipText();
    CString ExtractCoverImagePath();
    CString ExtractComicInfo();
    CString GetFileExtension();
    bool IsCBRFile();
    bool IsCBZFile();
};
