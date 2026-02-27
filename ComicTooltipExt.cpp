// ComicTooltipExt.cpp : Implementation of CComicTooltipExt

#include "pch.h"
#include <initguid.h>

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
DEFINE_GUID(CLSID_ComicTooltipExt,
    0xa1b2c3d4, 0xe5f6, 0x7890, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x92);

#include "ComicTooltipExt.h"
#include "ZipArchive.h"
#include "RarArchive.h"


// IPersist
STDMETHODIMP CComicTooltipExt::GetClassID(CLSID* pClassID)
{
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_ComicTooltipExt;
    return S_OK;
}

// IPersistFile
STDMETHODIMP CComicTooltipExt::Load(LPCOLESTR pszFileName, DWORD /*dwMode*/)
{
    if (!pszFileName) return E_INVALIDARG;
    m_strFileName = pszFileName;
    return S_OK;
}

STDMETHODIMP CComicTooltipExt::GetCurFile(LPOLESTR* ppszFileName)
{
    if (!ppszFileName) return E_POINTER;
    int nLen = m_strFileName.GetLength() + 1;
    *ppszFileName = static_cast<LPOLESTR>(::CoTaskMemAlloc(nLen * sizeof(WCHAR)));
    if (!*ppszFileName) return E_OUTOFMEMORY;
    wcscpy_s(*ppszFileName, nLen, m_strFileName);
    return S_OK;
}

// IQueryInfo
STDMETHODIMP CComicTooltipExt::GetInfoFlags(DWORD* pdwFlags)
{
    if (!pdwFlags) return E_POINTER;
    *pdwFlags = 0;
    return S_OK;
}

STDMETHODIMP CComicTooltipExt::GetInfoTip(DWORD /*dwFlags*/, LPWSTR* ppwszTipText)
{
    if (!ppwszTipText) return E_POINTER;

    CString strTip = GenerateTooltipText();

    int nLen = strTip.GetLength() + 1;
    *ppwszTipText = static_cast<LPWSTR>(::CoTaskMemAlloc(nLen * sizeof(WCHAR)));
    if (!*ppwszTipText) return E_OUTOFMEMORY;
    wcscpy_s(*ppwszTipText, nLen, strTip);
    return S_OK;
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

CString CComicTooltipExt::GetFileExtension()
{
    int nPos = m_strFileName.ReverseFind(L'.');
    return (nPos != -1) ? m_strFileName.Mid(nPos + 1) : CString();
}

bool CComicTooltipExt::IsCBRFile()
{
    return GetFileExtension().CompareNoCase(L"cbr") == 0;
}

bool CComicTooltipExt::IsCBZFile()
{
    return GetFileExtension().CompareNoCase(L"cbz") == 0;
}

CString CComicTooltipExt::ExtractCoverImagePath()
{
    std::wstring path;
    if (IsCBZFile())
    {
        ZipArchive zip;
        if (zip.Open(m_strFileName.GetString()))
        {
            std::wstring first = zip.FindFirstImageFile();
            if (!first.empty())
                path = zip.ExtractFileToTemp(first);
            zip.Close();
        }
    }
    else if (IsCBRFile())
    {
        RarArchive rar;
        if (rar.Open(m_strFileName.GetString()))
        {
            std::wstring first = rar.FindFirstImageFile();
            if (!first.empty())
                path = rar.ExtractFileToTemp(first);
            rar.Close();
        }
    }
    return CString(path.c_str());
}

CString CComicTooltipExt::ExtractComicInfo()
{
    std::wstring xmlContent;
    if (IsCBZFile())
    {
        ZipArchive zip;
        if (zip.Open(m_strFileName.GetString()))
        {
            xmlContent = zip.ExtractComicInfoXML();
            zip.Close();
        }
    }
    else if (IsCBRFile())
    {
        RarArchive rar;
        if (rar.Open(m_strFileName.GetString()))
        {
            xmlContent = rar.ExtractComicInfoXML();
            rar.Close();
        }
    }

    if (!xmlContent.empty())
    {
        ComicInfoParser parser;
        if (parser.ParseFromXML(xmlContent))
        {
            std::wstring text = parser.FormatAsText();
            if (!text.empty())
                return CString(text.c_str());
        }
    }

    // Fallback: no ComicInfo.xml - show nicely formatted file info
    static const wchar_t* kSepFB = L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500";

    CString strName = m_strFileName;
    int nPos = strName.ReverseFind(L'\\');
    if (nPos != -1) strName = strName.Mid(nPos + 1);
    nPos = strName.ReverseFind(L'.');
    if (nPos != -1) strName = strName.Left(nPos);

    CString strExt = GetFileExtension();
    strExt.MakeUpper();

    // Title
    CString strResult = strName;
    strResult += L"\r\n";

    // Type tag
    strResult += kSepFB;
    strResult += L"\r\n";
    if (strExt == L"CBZ")
        strResult += L"\u2022 Type:     Comic Book Archive (ZIP)\r\n";
    else if (strExt == L"CBR")
        strResult += L"\u2022 Type:     Comic Book Archive (RAR)\r\n";
    else
    {
        strResult += L"\u2022 Type:     ";
        strResult += strExt;
        strResult += L"\r\n";
    }

    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesEx(m_strFileName, GetFileExInfoStandard, &fa))
    {
        // File size
        ULARGE_INTEGER sz;
        sz.LowPart  = fa.nFileSizeLow;
        sz.HighPart = fa.nFileSizeHigh;
        CString strSize;
        if      (sz.QuadPart < 1024ULL)         strSize.Format(L"%u bytes",  sz.LowPart);
        else if (sz.QuadPart < 1024ULL * 1024)  strSize.Format(L"%.1f KB",   sz.QuadPart / 1024.0);
        else                                    strSize.Format(L"%.2f MB",   sz.QuadPart / (1024.0 * 1024.0));
        strResult += L"\u2022 Size:     ";
        strResult += strSize;
        strResult += L"\r\n";

        // Date modified
        FILETIME ftLocal;
        SYSTEMTIME st;
        if (FileTimeToLocalFileTime(&fa.ftLastWriteTime, &ftLocal) &&
            FileTimeToSystemTime(&ftLocal, &st))
        {
            CString strDate;
            strDate.Format(L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            strResult += L"\u2022 Modified: ";
            strResult += strDate;
            strResult += L"\r\n";
        }
    }

    // Remove trailing CRLF
    strResult.TrimRight(L"\r\n");
    return strResult;
}

CString CComicTooltipExt::GenerateTooltipText()
{
    // IQueryInfo plain-text tooltip – cover path on line 1 (used by preview handler),
    // then the comic metadata block.
    CString strCoverPath = ExtractCoverImagePath();
    CString strInfo      = ExtractComicInfo();

    CString strTip;
    if (!strCoverPath.IsEmpty())
        strTip = strCoverPath + L"\r\n" + strInfo;
    else
        strTip = strInfo;

    return strTip;
}
