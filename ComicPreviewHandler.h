// ComicPreviewHandler.h : Declaration of the preview handler for CBZ/CBR files

#pragma once
#include "resource.h"
#include "ComicInfo.h"

#include <ShObjIdl.h>   // IPreviewHandler, IInitializeWithFile

// {B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}
EXTERN_C const CLSID CLSID_ComicPreviewHandler;

class ATL_NO_VTABLE CComicPreviewHandler :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CComicPreviewHandler, &CLSID_ComicPreviewHandler>,
    public IPreviewHandler,
    public IInitializeWithFile,
    public IOleWindow,
    public IObjectWithSite
{
public:
    CComicPreviewHandler();
    ~CComicPreviewHandler();

    DECLARE_NOT_AGGREGATABLE(CComicPreviewHandler)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CComicPreviewHandler)
        COM_INTERFACE_ENTRY(IPreviewHandler)
        COM_INTERFACE_ENTRY(IInitializeWithFile)
        COM_INTERFACE_ENTRY(IOleWindow)
        COM_INTERFACE_ENTRY(IObjectWithSite)
    END_COM_MAP()

    // IInitializeWithFile
    STDMETHOD(Initialize)(LPCWSTR pszFilePath, DWORD grfMode);

    // IPreviewHandler
    STDMETHOD(SetWindow)(HWND hwnd, const RECT* prc);
    STDMETHOD(SetRect)(const RECT* prc);
    STDMETHOD(DoPreview)();
    STDMETHOD(Unload)();
    STDMETHOD(SetFocus)();
    STDMETHOD(QueryFocus)(HWND* phwnd);
    STDMETHOD(TranslateAccelerator)(MSG* pmsg);

    // IOleWindow
    STDMETHOD(GetWindow)(HWND* phwnd);
    STDMETHOD(ContextSensitiveHelp)(BOOL fEnterMode) { return E_NOTIMPL; }

    // IObjectWithSite
    STDMETHOD(SetSite)(IUnknown* punkSite);
    STDMETHOD(GetSite)(REFIID riid, void** ppvSite);

private:
    // State
    std::wstring    m_filePath;
    HWND            m_hwndParent;
    HWND            m_hwndPreview;
    RECT            m_rc;
    bool            m_bPreviewing;
    CComPtr<IUnknown> m_pSite;

    // Parsed data
    ComicInfoParser m_parser;
    bool            m_hasComicInfo;

    // Image data
    std::vector<BYTE>   m_coverData;
    std::vector<std::wstring> m_imageNames;   // all image file names in archive
    int                 m_totalPages;
    int                 m_currentPage;

    // Navigation button regions (populated during drawing, used for hit testing)
    RECT                m_prevButtonRect;
    RECT                m_nextButtonRect;

    // GDI+ token
    ULONG_PTR           m_gdiplusToken;

    // Methods
    void LoadArchiveData();
    bool LoadPage(int pageIndex);
    void NavigateToPage(int pageIndex);
    void NavigatePrevious();
    void NavigateNext();
    void CreatePreviewWindow();
    void DestroyPreviewWindow();
    void PaintContent(HDC hdc, const RECT& rc);
    void DrawCurrentPage(Gdiplus::Graphics& g, const Gdiplus::RectF& area);
    void DrawNavigationControls(Gdiplus::Graphics& g, const Gdiplus::RectF& imageArea);
    void DrawPageIndicator(Gdiplus::Graphics& g, const Gdiplus::RectF& imageArea);
    void DrawMetadata(Gdiplus::Graphics& g, const Gdiplus::RectF& area);
    void DrawPageStrip(Gdiplus::Graphics& g, const Gdiplus::RectF& area);

    bool IsCBZFile() const;
    bool IsCBRFile() const;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
