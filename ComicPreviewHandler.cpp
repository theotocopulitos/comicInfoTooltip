// ComicPreviewHandler.cpp : Implementation of CComicPreviewHandler

#include "pch.h"
#include "ComicPreviewHandler.h"
#include "ZipArchive.h"
#include "RarArchive.h"
#include "ArchiveDetect.h"

// {B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}
const CLSID CLSID_ComicPreviewHandler =
    { 0xb5e84a2f, 0x3d71, 0x4c8a, { 0x9f, 0x20, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f } };

using namespace Gdiplus;

// -----------------------------------------------------------------------
// Window class name
// -----------------------------------------------------------------------
static const wchar_t kWndClassName[] = L"ComicPreviewWnd_{B5E84A2F}";
static bool s_wndClassRegistered = false;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static bool IsImageFile(const std::wstring& name)
{
    static const wchar_t* kExts[] = { L".jpg", L".jpeg", L".png", L".gif", L".bmp", L".webp", nullptr };
    auto lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (int i = 0; kExts[i]; ++i)
    {
        size_t elen = wcslen(kExts[i]);
        if (lower.size() >= elen &&
            lower.compare(lower.size() - elen, elen, kExts[i]) == 0)
            return true;
    }
    return false;
}

static IStream* CreateStreamFromBytes(const std::vector<BYTE>& data)
{
    if (data.empty()) return nullptr;
    IStream* pStream = nullptr;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
    if (!hMem) return nullptr;
    void* pMem = GlobalLock(hMem);
    if (pMem)
    {
        memcpy(pMem, data.data(), data.size());
        GlobalUnlock(hMem);
        if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)))
            return pStream;
    }
    GlobalFree(hMem);
    return nullptr;
}

// -----------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------

CComicPreviewHandler::CComicPreviewHandler()
    : m_hwndParent(nullptr)
    , m_hwndPreview(nullptr)
    , m_bPreviewing(false)
    , m_hasComicInfo(false)
    , m_totalPages(0)
    , m_currentPage(0)
    , m_gdiplusToken(0)
{
    memset(&m_rc, 0, sizeof(m_rc));
    memset(&m_prevButtonRect, 0, sizeof(m_prevButtonRect));
    memset(&m_nextButtonRect, 0, sizeof(m_nextButtonRect));

    GdiplusStartupInput si;
    GdiplusStartup(&m_gdiplusToken, &si, nullptr);
}

CComicPreviewHandler::~CComicPreviewHandler()
{
    DestroyPreviewWindow();
    CloseArchive();
    if (m_gdiplusToken)
        GdiplusShutdown(m_gdiplusToken);
}

// -----------------------------------------------------------------------
// IInitializeWithFile
// -----------------------------------------------------------------------

STDMETHODIMP CComicPreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
    m_filePath = pszFilePath ? pszFilePath : L"";
    return S_OK;
}

// -----------------------------------------------------------------------
// IPreviewHandler
// -----------------------------------------------------------------------

STDMETHODIMP CComicPreviewHandler::SetWindow(HWND hwnd, const RECT* prc)
{
    m_hwndParent = hwnd;
    if (prc) m_rc = *prc;
    if (m_hwndPreview)
    {
        SetParent(m_hwndPreview, m_hwndParent);
        MoveWindow(m_hwndPreview, m_rc.left, m_rc.top,
                   m_rc.right - m_rc.left, m_rc.bottom - m_rc.top, TRUE);
    }
    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::SetRect(const RECT* prc)
{
    if (prc) m_rc = *prc;
    if (m_hwndPreview)
    {
        MoveWindow(m_hwndPreview, m_rc.left, m_rc.top,
                   m_rc.right - m_rc.left, m_rc.bottom - m_rc.top, TRUE);
        InvalidateRect(m_hwndPreview, nullptr, TRUE);
    }
    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::DoPreview()
{
    if (m_bPreviewing) return E_FAIL;
    m_bPreviewing = true;

    LoadArchiveData();
    CreatePreviewWindow();

    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::Unload()
{
    DestroyPreviewWindow();
    CloseArchive();
    m_coverData.clear();
    m_imageNames.clear();
    m_hasComicInfo = false;
    m_totalPages = 0;
    m_currentPage = 0;
    m_bPreviewing = false;
    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::SetFocus()
{
    if (m_hwndPreview) ::SetFocus(m_hwndPreview);
    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::QueryFocus(HWND* phwnd)
{
    if (!phwnd) return E_INVALIDARG;
    *phwnd = ::GetFocus();
    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::TranslateAccelerator(MSG* pmsg)
{
    return S_FALSE;
}

// -----------------------------------------------------------------------
// IOleWindow
// -----------------------------------------------------------------------

STDMETHODIMP CComicPreviewHandler::GetWindow(HWND* phwnd)
{
    if (!phwnd) return E_INVALIDARG;
    *phwnd = m_hwndPreview;
    return m_hwndPreview ? S_OK : E_FAIL;
}

// -----------------------------------------------------------------------
// IObjectWithSite
// -----------------------------------------------------------------------

STDMETHODIMP CComicPreviewHandler::SetSite(IUnknown* punkSite)
{
    m_pSite = punkSite;
    return S_OK;
}

STDMETHODIMP CComicPreviewHandler::GetSite(REFIID riid, void** ppvSite)
{
    if (!ppvSite) return E_INVALIDARG;
    *ppvSite = nullptr;
    if (!m_pSite) return E_FAIL;
    return m_pSite->QueryInterface(riid, ppvSite);
}

// -----------------------------------------------------------------------
// Archive helpers
// -----------------------------------------------------------------------

bool CComicPreviewHandler::IsCBZFile() const
{
    // Detect by magic bytes first, fall back to extension
    ArchiveFormat fmt = DetectArchiveFormat(m_filePath.c_str());
    if (fmt == ArchiveFormat::Zip) return true;
    if (fmt == ArchiveFormat::Rar) return false;
    auto ext = m_filePath;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext.size() >= 4 && ext.substr(ext.size() - 4) == L".cbz";
}

bool CComicPreviewHandler::IsCBRFile() const
{
    // Detect by magic bytes first, fall back to extension
    ArchiveFormat fmt = DetectArchiveFormat(m_filePath.c_str());
    if (fmt == ArchiveFormat::Rar) return true;
    if (fmt == ArchiveFormat::Zip) return false;
    auto ext = m_filePath;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext.size() >= 4 && ext.substr(ext.size() - 4) == L".cbr";
}

void CComicPreviewHandler::LoadArchiveData()
{
    CloseArchive();
    m_imageNames.clear();
    m_coverData.clear();
    m_hasComicInfo = false;
    m_totalPages = 0;
    m_currentPage = 0;

    if (IsCBZFile())
    {
        auto zip = std::make_unique<ZipArchive>();
        if (zip->Open(m_filePath))
        {
            auto files = zip->GetFileList();
            for (auto& f : files)
            {
                if (!f.IsDirectory && IsImageFile(f.FileName))
                    m_imageNames.push_back(f.FileName);
            }
            std::sort(m_imageNames.begin(), m_imageNames.end());
            m_totalPages = (int)m_imageNames.size();

            std::wstring xmlContent = zip->ExtractComicInfoXML();
            if (!xmlContent.empty())
                m_hasComicInfo = m_parser.ParseFromXML(xmlContent);

            m_pZip = std::move(zip);
        }
    }
    else if (IsCBRFile())
    {
        auto rar = std::make_unique<RarArchive>();
        if (rar->Open(m_filePath))
        {
            auto files = rar->GetFileList();
            for (auto& f : files)
            {
                if (!f.IsDirectory && IsImageFile(f.FileName))
                    m_imageNames.push_back(f.FileName);
            }
            std::sort(m_imageNames.begin(), m_imageNames.end());
            m_totalPages = (int)m_imageNames.size();

            std::wstring xmlContent = rar->ExtractComicInfoXML();
            if (!xmlContent.empty())
                m_hasComicInfo = m_parser.ParseFromXML(xmlContent);

            m_pRar = std::move(rar);
        }
    }

    // Load the first page; if it fails (e.g. the entry is corrupt), try subsequent
    // pages so the preview still shows something rather than a blank placeholder.
    for (int i = 0; i < m_totalPages; ++i)
    {
        if (LoadPage(i))
        {
            m_currentPage = i;
            break;
        }
    }
}

void CComicPreviewHandler::CloseArchive()
{
    m_pZip.reset();
    m_pRar.reset();
}

bool CComicPreviewHandler::LoadPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= (int)m_imageNames.size())
        return false;

    m_coverData.clear();

    if (m_pZip && m_pZip->IsOpen())
    {
        m_coverData = m_pZip->ExtractFile(m_imageNames[pageIndex]);
    }
    else if (m_pRar && m_pRar->IsOpen())
    {
        m_coverData = m_pRar->ExtractFile(m_imageNames[pageIndex]);
    }
    else
    {
        return false;
    }

    if (m_hwndPreview)
        InvalidateRect(m_hwndPreview, nullptr, TRUE);

    return !m_coverData.empty();
}

// -----------------------------------------------------------------------
// Window management
// -----------------------------------------------------------------------

void CComicPreviewHandler::CreatePreviewWindow()
{
    if (!m_hwndParent) return;

    HINSTANCE hInst = ATL::_AtlBaseModule.GetModuleInstance();

    if (!s_wndClassRegistered)
    {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
        wc.lpfnWndProc   = WndProc;
        wc.hInstance      = hInst;
        wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName  = kWndClassName;
        RegisterClassEx(&wc);
        s_wndClassRegistered = true;
    }

    m_hwndPreview = CreateWindowEx(
        0, kWndClassName, L"Comic Preview",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP,
        m_rc.left, m_rc.top,
        m_rc.right - m_rc.left, m_rc.bottom - m_rc.top,
        m_hwndParent, nullptr, hInst, this);
}

void CComicPreviewHandler::DestroyPreviewWindow()
{
    if (m_hwndPreview)
    {
        DestroyWindow(m_hwndPreview);
        m_hwndPreview = nullptr;
    }
}

// -----------------------------------------------------------------------
// Navigation
// -----------------------------------------------------------------------

void CComicPreviewHandler::NavigateToPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_totalPages) return;
    LoadPage(pageIndex);
    m_currentPage = pageIndex;
}

void CComicPreviewHandler::NavigatePrevious()
{
    NavigateToPage(m_currentPage - 1);
}

void CComicPreviewHandler::NavigateNext()
{
    NavigateToPage(m_currentPage + 1);
}

LRESULT CALLBACK CComicPreviewHandler::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    CComicPreviewHandler* pThis = nullptr;

    if (msg == WM_CREATE)
    {
        auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lp);
        pThis = reinterpret_cast<CComicPreviewHandler*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CComicPreviewHandler*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (msg)
    {
    case WM_PAINT:
    {
        if (!pThis) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        pThis->PaintContent(hdc, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;   // we paint everything
    case WM_KEYDOWN:
    {
        if (!pThis) break;
        switch (wp)
        {
        case VK_LEFT:
        case VK_UP:
            pThis->NavigatePrevious();
            return 0;
        case VK_RIGHT:
        case VK_DOWN:
            pThis->NavigateNext();
            return 0;
        case VK_HOME:
            pThis->NavigateToPage(0);
            return 0;
        case VK_END:
            pThis->NavigateToPage(pThis->m_totalPages - 1);
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        if (!pThis) break;
        ::SetFocus(hwnd);

        if (pThis->m_totalPages > 1)
        {
            int xClick = GET_X_LPARAM(lp);
            int yClick = GET_Y_LPARAM(lp);
            POINT pt = { xClick, yClick };
            if (PtInRect(&pThis->m_prevButtonRect, pt))
            {
                pThis->NavigatePrevious();
                return 0;
            }
            if (PtInRect(&pThis->m_nextButtonRect, pt))
            {
                pThis->NavigateNext();
                return 0;
            }
        }
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

// -----------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------

static const Color kBgColor(255, 248, 248, 250);         // light bg
static const Color kCardBg(255, 255, 255, 255);          // white card
static const Color kAccent(255, 40, 100, 210);           // blue accent
static const Color kTextPrimary(255, 30, 30, 35);        // dark text
static const Color kTextSecondary(255, 60, 60, 70);      // medium text
static const Color kTextLabel(255, 120, 120, 130);       // dim label
static const Color kSeparator(255, 210, 210, 215);       // light separator
static const Color kStarFilled(255, 230, 170, 20);       // gold star
static const Color kStarEmpty(255, 200, 200, 205);       // light star

void CComicPreviewHandler::PaintContent(HDC hdc, const RECT& rc)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    // Render into a GDI+ Bitmap at exact pixel dimensions (avoids DPI scaling)
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    bmp.SetResolution(96.0f, 96.0f);

    Graphics g(&bmp);
    g.SetPageUnit(UnitPixel);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(kBgColor);
    g.FillRectangle(&bgBrush, 0, 0, w, h);

    // Layout: cover on left, metadata on right
    float margin = 12.0f;
    float gap = 8.0f;
    float coverWidth = (float)w * 0.4f;
    if (coverWidth > 400.0f) coverWidth = 400.0f;

    RectF coverArea(margin, margin, coverWidth - margin, (float)h - margin * 2);
    float metaX = coverWidth + gap;
    float metaW = (float)w - metaX - margin;
    if (metaW < 100) metaW = 100;
    RectF metaArea(metaX, margin, metaW, (float)h - margin * 2);

    DrawCurrentPage(g, coverArea);
    DrawNavigationControls(g, coverArea);
    DrawPageIndicator(g, coverArea);
    DrawMetadata(g, metaArea);

    // Blit the bitmap to the target DC
    Graphics gDst(hdc);
    gDst.SetPageUnit(UnitPixel);
    gDst.DrawImage(&bmp, rc.left, rc.top, w, h);
}

void CComicPreviewHandler::DrawCurrentPage(Graphics& g, const RectF& area)
{
    if (m_coverData.empty())
    {
        // Draw "No Cover" placeholder
        SolidBrush noBrush(kTextLabel);
        Font noFont(L"Segoe UI", 13, FontStyleItalic, UnitPixel);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(L"No cover image", -1, &noFont, area, &sf, &noBrush);
        return;
    }

    IStream* pStream = CreateStreamFromBytes(m_coverData);
    if (!pStream) return;

    Image img(pStream);
    Status st = img.GetLastStatus();
    pStream->Release();

    if (st != Ok) return;

    float imgW = (float)img.GetWidth();
    float imgH = (float)img.GetHeight();
    if (imgW <= 0 || imgH <= 0) return;

    // Fit to area maintaining aspect ratio
    float scaleX = area.Width / imgW;
    float scaleY = area.Height / imgH;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    float drawW = imgW * scale;
    float drawH = imgH * scale;
    float drawX = area.X + (area.Width - drawW) / 2.0f;
    float drawY = area.Y;  // top-aligned

    // Shadow
    SolidBrush shadowBrush(Color(40, 0, 0, 0));
    g.FillRectangle(&shadowBrush, drawX + 3, drawY + 3, drawW, drawH);

    // Image
    g.DrawImage(&img, drawX, drawY, drawW, drawH);

    // Thin border
    Pen borderPen(Color(60, 0, 0, 0), 1.0f);
    g.DrawRectangle(&borderPen, drawX, drawY, drawW, drawH);
}

void CComicPreviewHandler::DrawNavigationControls(Graphics& g, const RectF& imageArea)
{
    if (m_totalPages <= 1) return;

    // Button dimensions
    const float btnW = 28.0f;
    const float btnH = 48.0f;
    const float btnY = imageArea.Y + (imageArea.Height - btnH) / 2.0f;

    const float prevX = imageArea.X;
    const float nextX = imageArea.X + imageArea.Width - btnW;

    // Semi-transparent button backgrounds
    SolidBrush btnBg(Color(160, 0, 0, 0));
    SolidBrush arrowBrush(Color(255, 255, 255, 255));
    Font arrowFont(L"Segoe UI Symbol", 14, FontStyleBold, UnitPixel);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);

    // Previous button
    if (m_currentPage > 0)
    {
        RectF prevRect(prevX, btnY, btnW, btnH);
        g.FillRectangle(&btnBg, prevRect);
        g.DrawString(L"\u25C0", -1, &arrowFont, prevRect, &sf, &arrowBrush);

        // Store button region for hit testing
        m_prevButtonRect.left   = (LONG)(imageArea.X);
        m_prevButtonRect.top    = (LONG)btnY;
        m_prevButtonRect.right  = (LONG)(imageArea.X + btnW);
        m_prevButtonRect.bottom = (LONG)(btnY + btnH);
    }
    else
    {
        memset(&m_prevButtonRect, 0, sizeof(m_prevButtonRect));
    }

    // Next button
    if (m_currentPage < m_totalPages - 1)
    {
        RectF nextRect(nextX, btnY, btnW, btnH);
        g.FillRectangle(&btnBg, nextRect);
        g.DrawString(L"\u25B6", -1, &arrowFont, nextRect, &sf, &arrowBrush);

        // Store button region for hit testing
        m_nextButtonRect.left   = (LONG)nextX;
        m_nextButtonRect.top    = (LONG)btnY;
        m_nextButtonRect.right  = (LONG)(nextX + btnW);
        m_nextButtonRect.bottom = (LONG)(btnY + btnH);
    }
    else
    {
        memset(&m_nextButtonRect, 0, sizeof(m_nextButtonRect));
    }
}

void CComicPreviewHandler::DrawPageIndicator(Graphics& g, const RectF& imageArea)
{
    if (m_totalPages <= 0) return;

    FontFamily ff(L"Segoe UI");
    Font indicatorFont(&ff, 12, FontStyleRegular, UnitPixel);

    wchar_t buf[32];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d / %d", m_currentPage + 1, m_totalPages);

    // Measure text
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    RectF measureRect(0, 0, 200, 30);
    RectF measured;
    g.MeasureString(buf, -1, &indicatorFont, measureRect, &sf, &measured);

    float padX = 8.0f;
    float padY = 3.0f;
    float bgW = measured.Width + padX * 2;
    float bgH = measured.Height + padY * 2;
    float bgX = imageArea.X + (imageArea.Width - bgW) / 2.0f;
    float bgY = imageArea.Y + imageArea.Height - bgH - 6.0f;

    // Semi-transparent background
    SolidBrush bgBrush(Color(160, 0, 0, 0));
    g.FillRectangle(&bgBrush, bgX, bgY, bgW, bgH);

    // Text
    SolidBrush textBrush(Color(255, 255, 255, 255));
    RectF textRect(bgX, bgY, bgW, bgH);
    StringFormat centerSf;
    centerSf.SetAlignment(StringAlignmentCenter);
    centerSf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(buf, -1, &indicatorFont, textRect, &centerSf, &textBrush);
}

// Helper: draw a label:value pair, returns Y advance
static float DrawField(Graphics& g, const Font* labelFont, const Font* valueFont,
                       float x, float y, float maxWidth,
                       const wchar_t* label, const std::wstring& value)
{
    if (value.empty()) return 0;

    SolidBrush labelBrush(kTextLabel);
    SolidBrush valueBrush(kTextSecondary);
    StringFormat sf;
    sf.SetTrimming(StringTrimmingEllipsisCharacter);

    RectF labelRect(x, y, 110, 20);
    g.DrawString(label, -1, labelFont, labelRect, &sf, &labelBrush);

    RectF valueRect(x + 110, y, maxWidth - 110, 20);
    g.DrawString(value.c_str(), -1, valueFont, valueRect, &sf, &valueBrush);

    return 22.0f;
}

// Draw stars for rating
static float DrawStars(Graphics& g, float x, float y, const std::wstring& ratingStr)
{
    if (ratingStr.empty()) return 0;
    double rating = 0;
    try { rating = std::stod(ratingStr); } catch (...) { return 0; }
    if (rating < 0 || rating > 5) return 0;

    SolidBrush filled(kStarFilled);
    SolidBrush empty(kStarEmpty);
    Font starFont(L"Segoe UI", 16, FontStyleRegular, UnitPixel);
    StringFormat sf;

    for (int i = 0; i < 5; i++)
    {
        PointF pt(x + i * 20.0f, y);
        if (i < (int)rating)
            g.DrawString(L"\u2605", -1, &starFont, pt, &sf, &filled);
        else
            g.DrawString(L"\u2606", -1, &starFont, pt, &sf, &empty);
    }

    // Numeric value
    SolidBrush numBrush(kTextSecondary);
    Font numFont(L"Segoe UI", 12, FontStyleRegular, UnitPixel);
    wchar_t buf[16];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"  %.1f / 5", rating);
    PointF numPt(x + 105, y + 2);
    g.DrawString(buf, -1, &numFont, numPt, &sf, &numBrush);

    return 26.0f;
}

void CComicPreviewHandler::DrawMetadata(Graphics& g, const RectF& area)
{
    float x = area.X + 8;
    float y = area.Y;
    float maxW = area.Width - 16;

    FontFamily ff(L"Segoe UI");
    Font titleFont(&ff, 20, FontStyleBold, UnitPixel);
    Font subtitleFont(&ff, 14, FontStyleItalic, UnitPixel);
    Font sectionFont(&ff, 13, FontStyleBold, UnitPixel);
    Font labelFont(&ff, 12, FontStyleRegular, UnitPixel);
    Font valueFont(&ff, 12, FontStyleRegular, UnitPixel);
    Font smallFont(&ff, 11, FontStyleRegular, UnitPixel);

    SolidBrush titleBrush(kTextPrimary);
    SolidBrush subtitleBrush(kTextSecondary);
    SolidBrush accentBrush(kAccent);
    SolidBrush labelBrush(kTextLabel);
    Pen sepPen(kSeparator, 1.0f);
    StringFormat sf;
    sf.SetTrimming(StringTrimmingEllipsisCharacter);

    const auto& d = m_hasComicInfo ? m_parser.GetData() : ComicInfoData();

    // ── Title ──
    std::wstring mainTitle;
    if (!d.Series.empty())
    {
        mainTitle = d.Series;
        if (!d.Volume.empty() && d.Volume != L"-1")
            mainTitle += L" \u2022 Vol. " + d.Volume;
        if (!d.Number.empty())
        {
            mainTitle += L" #" + d.Number;
            if (!d.Count.empty() && d.Count != L"-1")
                mainTitle += L" of " + d.Count;
        }
    }
    else if (!d.Title.empty())
    {
        mainTitle = d.Title;
    }
    else
    {
        // Use filename as title
        mainTitle = m_filePath;
        size_t pos = mainTitle.rfind(L'\\');
        if (pos != std::wstring::npos) mainTitle = mainTitle.substr(pos + 1);
        pos = mainTitle.rfind(L'.');
        if (pos != std::wstring::npos) mainTitle = mainTitle.substr(0, pos);
    }

    RectF titleRect(x, y, maxW, 50);
    g.DrawString(mainTitle.c_str(), -1, &titleFont, titleRect, &sf, &titleBrush);
    y += 30;

    // Episode title
    if (!d.Series.empty() && !d.Title.empty())
    {
        std::wstring sub = L"\u201c" + d.Title + L"\u201d";
        RectF subRect(x, y, maxW, 24);
        g.DrawString(sub.c_str(), -1, &subtitleFont, subRect, &sf, &subtitleBrush);
        y += 22;
    }

    // Alternate series
    if (!d.AlternateSeries.empty())
    {
        std::wstring alt = L"(" + d.AlternateSeries;
        if (!d.AlternateNumber.empty()) alt += L" #" + d.AlternateNumber;
        alt += L")";
        RectF altRect(x, y, maxW, 20);
        g.DrawString(alt.c_str(), -1, &smallFont, altRect, &sf, &labelBrush);
        y += 18;
    }

    // Rating
    if (!d.CommunityRating.empty())
    {
        y += DrawStars(g, x, y, d.CommunityRating);
    }

    y += 8;
    g.DrawLine(&sepPen, x, y, x + maxW, y);
    y += 8;

    // ── Publication section ──
    {
        RectF secRect(x, y, maxW, 20);
        g.DrawString(L"PUBLICATION", -1, &sectionFont, secRect, &sf, &accentBrush);
        y += 22;
    }

    if (!d.Publisher.empty())
    {
        std::wstring pub = d.Publisher;
        if (!d.Imprint.empty()) pub += L" / " + d.Imprint;
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Publisher:", pub);
    }
    else if (!d.Imprint.empty())
    {
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Imprint:", d.Imprint);
    }

    // Date
    std::wstring dateStr;
    if (!d.Year.empty() && d.Year != L"-1")
    {
        dateStr = d.Year;
        if (!d.Month.empty() && d.Month != L"-1")
        {
            dateStr += L"-" + (d.Month.length() == 1 ? L"0" + d.Month : d.Month);
            if (!d.Day.empty() && d.Day != L"-1")
                dateStr += L"-" + (d.Day.length() == 1 ? L"0" + d.Day : d.Day);
        }
    }
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Date:", dateStr);
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Genre:", d.Genre);
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Tags:", d.Tags);
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Format:", d.Format);
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Language:", d.LanguageISO);

    if (!d.AgeRating.empty() && d.AgeRating != L"Unknown")
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Age Rating:", d.AgeRating);
    if (!d.PageCount.empty() && d.PageCount != L"0" && d.PageCount != L"-1")
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Pages:", d.PageCount);

    // Flags
    std::wstring flags;
    if (d.BlackAndWhite == L"Yes") flags += L"B&W";
    if (d.Manga == L"Yes" || d.Manga == L"YesAndRightToLeft")
    {
        if (!flags.empty()) flags += L", ";
        flags += (d.Manga == L"YesAndRightToLeft") ? L"Manga (RTL)" : L"Manga";
    }
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Flags:", flags);
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"ISBN/GTIN:", d.GTIN);
    y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Web:", d.Web);

    // ── Creative team section ──
    bool hasCrew = !d.Writer.empty() || !d.Penciller.empty() || !d.Inker.empty()
                || !d.Colorist.empty() || !d.Letterer.empty() || !d.CoverArtist.empty()
                || !d.Editor.empty() || !d.Translator.empty();
    if (hasCrew)
    {
        y += 4;
        g.DrawLine(&sepPen, x, y, x + maxW, y);
        y += 8;
        RectF secRect(x, y, maxW, 20);
        g.DrawString(L"CREATIVE TEAM", -1, &sectionFont, secRect, &sf, &accentBrush);
        y += 22;

        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Writer:", d.Writer);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Penciller:", d.Penciller);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Inker:", d.Inker);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Colorist:", d.Colorist);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Letterer:", d.Letterer);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Cover Art:", d.CoverArtist);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Editor:", d.Editor);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Translator:", d.Translator);
    }

    // ── Story / Characters section ──
    bool hasStory = !d.StoryArc.empty() || !d.Characters.empty() || !d.Teams.empty()
                 || !d.Locations.empty() || !d.SeriesGroup.empty() || !d.MainCharacterOrTeam.empty();
    if (hasStory)
    {
        y += 4;
        g.DrawLine(&sepPen, x, y, x + maxW, y);
        y += 8;
        RectF secRect(x, y, maxW, 20);
        g.DrawString(L"STORY", -1, &sectionFont, secRect, &sf, &accentBrush);
        y += 22;

        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Main:", d.MainCharacterOrTeam);
        if (!d.StoryArc.empty())
        {
            std::wstring arc = d.StoryArc;
            if (!d.StoryArcNumber.empty()) arc += L" (Part " + d.StoryArcNumber + L")";
            y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Story Arc:", arc);
        }
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Characters:", d.Characters);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Teams:", d.Teams);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Locations:", d.Locations);
        y += DrawField(g, &labelFont, &valueFont, x, y, maxW, L"Series Grp:", d.SeriesGroup);
    }

    // ── Summary ──
    if (!d.Summary.empty())
    {
        y += 4;
        g.DrawLine(&sepPen, x, y, x + maxW, y);
        y += 8;

        RectF secRect(x, y, maxW, 20);
        g.DrawString(L"SUMMARY", -1, &sectionFont, secRect, &sf, &accentBrush);
        y += 22;

        // Word-wrap the summary
        RectF sumRect(x, y, maxW, area.Height - (y - area.Y));
        StringFormat wrapFmt;
        wrapFmt.SetTrimming(StringTrimmingEllipsisCharacter);
        SolidBrush sumBrush(kTextSecondary);
        g.DrawString(d.Summary.c_str(), -1, &valueFont, sumRect, &wrapFmt, &sumBrush);

        RectF measured;
        g.MeasureString(d.Summary.c_str(), -1, &valueFont, sumRect, &wrapFmt, &measured);
        y += measured.Height + 4;
    }

    // ── Notes ──
    if (!d.Notes.empty())
    {
        y += 4;
        g.DrawLine(&sepPen, x, y, x + maxW, y);
        y += 8;
        RectF secRect(x, y, maxW, 20);
        g.DrawString(L"NOTES", -1, &sectionFont, secRect, &sf, &accentBrush);
        y += 22;
        RectF nRect(x, y, maxW, area.Height - (y - area.Y));
        StringFormat wf;
        wf.SetTrimming(StringTrimmingEllipsisCharacter);
        SolidBrush nBrush(kTextSecondary);
        g.DrawString(d.Notes.c_str(), -1, &smallFont, nRect, &wf, &nBrush);
    }

    // ── Scan info at very bottom ──
    if (!d.ScanInformation.empty())
    {
        SolidBrush scanBrush(kTextLabel);
        RectF scanRect(x, area.Y + area.Height - 20, maxW, 18);
        g.DrawString(d.ScanInformation.c_str(), -1, &smallFont, scanRect, &sf, &scanBrush);
    }

    // ── Page count badge (bottom-left of metadata area) ──
    if (m_totalPages > 0)
    {
        wchar_t pageBuf[64];
        _snwprintf_s(pageBuf, _countof(pageBuf), _TRUNCATE, L"%d pages in archive", m_totalPages);
        SolidBrush pageBrush(kTextLabel);
        RectF pageRect(x, area.Y + area.Height - 38, maxW, 18);
        g.DrawString(pageBuf, -1, &smallFont, pageRect, &sf, &pageBrush);
    }
}

void CComicPreviewHandler::DrawPageStrip(Graphics& g, const RectF& area)
{
    // Placeholder for future page browser thumbnail strip
}
