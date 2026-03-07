// RarArchive.cpp : CBR (RAR) file handling implementation
//
// Uses the UnRAR DLL (unrar.dll) loaded dynamically at runtime to support
// both RAR4 and RAR5 archives.  If unrar.dll is absent the archive returns
// an empty file list, mirroring the previous fallback behaviour.

#include "pch.h"
#include "RarArchive.h"
#include "dll.hpp"

// -----------------------------------------------------------------------
// Dynamic UnRAR DLL loader
// -----------------------------------------------------------------------

struct UnRarDll
{
    HMODULE              hDll            = NULL;
    PFN_RAROpenArchiveEx pfnOpenArchiveEx = nullptr;
    PFN_RARCloseArchive  pfnCloseArchive  = nullptr;
    PFN_RARReadHeaderEx  pfnReadHeaderEx  = nullptr;
    PFN_RARProcessFileW  pfnProcessFileW  = nullptr;
    PFN_RARSetCallback   pfnSetCallback   = nullptr;

    bool Load()
    {
        if (hDll) return true;

        hDll = ::LoadLibraryW(L"unrar.dll");
        if (!hDll) return false;

        pfnOpenArchiveEx = reinterpret_cast<PFN_RAROpenArchiveEx>(
            ::GetProcAddress(hDll, "RAROpenArchiveEx"));
        pfnCloseArchive  = reinterpret_cast<PFN_RARCloseArchive>(
            ::GetProcAddress(hDll, "RARCloseArchive"));
        pfnReadHeaderEx  = reinterpret_cast<PFN_RARReadHeaderEx>(
            ::GetProcAddress(hDll, "RARReadHeaderEx"));
        pfnProcessFileW  = reinterpret_cast<PFN_RARProcessFileW>(
            ::GetProcAddress(hDll, "RARProcessFileW"));
        pfnSetCallback   = reinterpret_cast<PFN_RARSetCallback>(
            ::GetProcAddress(hDll, "RARSetCallback"));

        if (!pfnOpenArchiveEx || !pfnCloseArchive || !pfnReadHeaderEx ||
            !pfnProcessFileW  || !pfnSetCallback)
        {
            ::FreeLibrary(hDll);
            hDll            = NULL;
            pfnOpenArchiveEx = nullptr;
            pfnCloseArchive  = nullptr;
            pfnReadHeaderEx  = nullptr;
            pfnProcessFileW  = nullptr;
            pfnSetCallback   = nullptr;
            return false;
        }
        return true;
    }

    ~UnRarDll()
    {
        if (hDll) { ::FreeLibrary(hDll); hDll = NULL; }
    }
};

static UnRarDll g_unrar;

// -----------------------------------------------------------------------
// UCM_PROCESSDATA callback — accumulates decompressed bytes
// -----------------------------------------------------------------------

struct ExtractionContext
{
    std::vector<BYTE>* pData = nullptr;
};

static int CALLBACK UnRarCallback(UINT msg, LPARAM userData, LPARAM p1, LPARAM p2)
{
    if (msg == UCM_PROCESSDATA)
    {
        ExtractionContext* ctx = reinterpret_cast<ExtractionContext*>(userData);
        if (ctx && ctx->pData)
        {
            const BYTE* ptr  = reinterpret_cast<const BYTE*>(p1);
            size_t      size = static_cast<size_t>(p2);
            ctx->pData->insert(ctx->pData->end(), ptr, ptr + size);
        }
    }
    return 1;
}

// -----------------------------------------------------------------------
// Helpers shared by ParseRarHeader / ReadRarFileHeader
// -----------------------------------------------------------------------

// Convert a RARHeaderDataEx filename to a normalised wide string.
static std::wstring HeaderFileName(const RARHeaderDataEx& hdr)
{
    std::wstring name;
    if (hdr.FileNameW[0])
    {
        name = hdr.FileNameW;
    }
    else
    {
        // Filenames without a Unicode counterpart are encoded in the archive's
        // host ANSI code page (CP_ACP), which is what the RAR4 spec requires.
        int wlen = ::MultiByteToWideChar(CP_ACP, 0, hdr.FileName, -1, NULL, 0);
        if (wlen > 1)
        {
            name.resize(wlen - 1);
            ::MultiByteToWideChar(CP_ACP, 0, hdr.FileName, -1, &name[0], wlen);
        }
    }
    std::replace(name.begin(), name.end(), L'\\', L'/');
    return name;
}

// -----------------------------------------------------------------------
// RarArchive implementation
// -----------------------------------------------------------------------

RarArchive::RarArchive() : m_hFile(INVALID_HANDLE_VALUE) {}

RarArchive::~RarArchive() { Close(); }

bool RarArchive::Open(const std::wstring& filePath)
{
    if (m_hFile != INVALID_HANDLE_VALUE) Close();

    m_hFile = ::CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) return false;

    m_filePath = filePath;
    if (!ParseRarHeader())
    {
        Close();
        return false;
    }
    return true;
}

void RarArchive::Close()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
    CleanupTempFiles();
    m_fileList.clear();
}

std::vector<RarFileInfo> RarArchive::GetFileList()
{
    return m_fileList;
}

std::vector<BYTE> RarArchive::ExtractFile(const std::wstring& fileName)
{
    std::vector<BYTE> result;
    for (const auto& fi : m_fileList)
    {
        if (!fi.IsDirectory && _wcsicmp(fi.FileName.c_str(), fileName.c_str()) == 0)
        {
            ReadRarFileHeader(fi, result);
            break;
        }
    }
    return result;
}

std::wstring RarArchive::ExtractFileToTemp(const std::wstring& fileName)
{
    std::vector<BYTE> data = ExtractFile(fileName);
    if (data.empty()) return L"";

    wchar_t tmpDir[MAX_PATH], tmpFile[MAX_PATH];
    if (!::GetTempPathW(MAX_PATH, tmpDir))          return L"";
    if (!::GetTempFileNameW(tmpDir, L"cbr", 0, tmpFile)) return L"";

    std::wstring ext;
    size_t dot = fileName.rfind(L'.');
    if (dot != std::wstring::npos) ext = fileName.substr(dot);

    std::wstring finalPath = tmpFile;
    if (!ext.empty())
    {
        finalPath += ext;
        ::DeleteFileW(tmpFile);
    }

    HANDLE hOut = ::CreateFileW(finalPath.c_str(), GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) return L"";

    DWORD written = 0;
    ::WriteFile(hOut, data.data(), static_cast<DWORD>(data.size()), &written, NULL);
    ::CloseHandle(hOut);

    m_tempFiles.push_back(finalPath);
    return finalPath;
}

std::wstring RarArchive::FindFirstImageFile()
{
    static const wchar_t* kExts[] = { L".jpg", L".jpeg", L".png", L".gif", L".bmp", L".webp", nullptr };

    std::vector<RarFileInfo> sorted = m_fileList;
    std::sort(sorted.begin(), sorted.end(),
              [](const RarFileInfo& a, const RarFileInfo& b){ return a.FileName < b.FileName; });

    for (const auto& fi : sorted)
    {
        if (fi.IsDirectory) continue;
        std::wstring lower = fi.FileName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        for (int i = 0; kExts[i]; ++i)
        {
            size_t elen = wcslen(kExts[i]);
            if (lower.size() >= elen &&
                lower.compare(lower.size() - elen, elen, kExts[i]) == 0)
                return fi.FileName;
        }
    }
    return L"";
}

bool RarArchive::HasComicInfoXML()
{
    for (const auto& fi : m_fileList)
        if (_wcsicmp(fi.FileName.c_str(), L"ComicInfo.xml") == 0) return true;
    return false;
}

std::wstring RarArchive::ExtractComicInfoXML()
{
    std::wstring entryName;
    for (const auto& fi : m_fileList)
    {
        if (_wcsicmp(fi.FileName.c_str(), L"ComicInfo.xml") == 0)
        {
            entryName = fi.FileName;
            break;
        }
    }
    if (entryName.empty()) return L"";

    std::vector<BYTE> raw = ExtractFile(entryName);
    if (raw.empty()) return L"";

    int wlen = ::MultiByteToWideChar(CP_UTF8, 0,
                                     reinterpret_cast<const char*>(raw.data()),
                                     static_cast<int>(raw.size()), NULL, 0);
    if (wlen <= 0) return L"";
    std::wstring result(wlen, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0,
                          reinterpret_cast<const char*>(raw.data()),
                          static_cast<int>(raw.size()), &result[0], wlen);
    return result;
}

// -----------------------------------------------------------------------
// UnRAR SDK — file listing
// -----------------------------------------------------------------------

bool RarArchive::ParseRarHeader()
{
    if (!g_unrar.Load()) return false;

    RAROpenArchiveDataEx arcData = {};
    // ArcNameW is declared as wchar_t* in the SDK (no const), but the SDK
    // does not modify the path — the const_cast is safe here.
    arcData.ArcNameW  = const_cast<wchar_t*>(m_filePath.c_str());
    arcData.OpenMode  = RAR_OM_LIST;

    HANDLE hArc = g_unrar.pfnOpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != ERAR_SUCCESS)
    {
        if (hArc) g_unrar.pfnCloseArchive(hArc);
        return false;
    }

    RARHeaderDataEx hdr = {};
    int ret;
    while ((ret = g_unrar.pfnReadHeaderEx(hArc, &hdr)) == ERAR_SUCCESS)
    {
        RarFileInfo fi   = {};
        fi.FileName      = HeaderFileName(hdr);
        // RarFileInfo.FileSize is DWORD (32-bit); images in comic archives
        // are never ≥ 4 GB so storing the lower 32 bits of UnpSize is safe.
        fi.FileSize      = hdr.UnpSize;
        fi.CompressedSize = hdr.PackSize;
        fi.CRC32         = hdr.FileCRC;
        fi.FileOffset    = 0;                 // not used with UnRAR SDK
        fi.IsDirectory   = (hdr.FileAttr & 0x10) != 0;

        m_fileList.push_back(fi);

        // Must call ProcessFileW to advance the iterator; RAR_SKIP is free
        g_unrar.pfnProcessFileW(hArc, RAR_SKIP, NULL, NULL);
    }

    g_unrar.pfnCloseArchive(hArc);
    return !m_fileList.empty();
}

// -----------------------------------------------------------------------
// UnRAR SDK — file extraction (callback accumulates decompressed data)
// -----------------------------------------------------------------------

bool RarArchive::ReadRarFileHeader(const RarFileInfo& fileInfo, std::vector<BYTE>& data)
{
    if (!g_unrar.Load()) return false;

    ExtractionContext ctx;
    ctx.pData = &data;
    if (fileInfo.FileSize > 0)
        data.reserve(fileInfo.FileSize);

    RAROpenArchiveDataEx arcData = {};
    // ArcNameW is declared as wchar_t* in the SDK (no const), but the SDK
    // does not modify the path — the const_cast is safe here.
    arcData.ArcNameW  = const_cast<wchar_t*>(m_filePath.c_str());
    arcData.OpenMode  = RAR_OM_EXTRACT;
    arcData.Callback  = UnRarCallback;
    arcData.UserData  = reinterpret_cast<LPARAM>(&ctx);

    HANDLE hArc = g_unrar.pfnOpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != ERAR_SUCCESS)
    {
        if (hArc) g_unrar.pfnCloseArchive(hArc);
        return false;
    }

    bool found = false;
    RARHeaderDataEx hdr = {};
    int ret;
    while ((ret = g_unrar.pfnReadHeaderEx(hArc, &hdr)) == ERAR_SUCCESS)
    {
        std::wstring name = HeaderFileName(hdr);
        // Case-insensitive match to be consistent with ExtractFile() above.
        if (_wcsicmp(name.c_str(), fileInfo.FileName.c_str()) == 0)
        {
            // RAR_TEST decompresses into the callback; nothing is written to disk
            g_unrar.pfnProcessFileW(hArc, RAR_TEST, NULL, NULL);
            found = true;
            break;
        }
        g_unrar.pfnProcessFileW(hArc, RAR_SKIP, NULL, NULL);
    }

    g_unrar.pfnCloseArchive(hArc);
    return found && !data.empty();
}

// -----------------------------------------------------------------------
// Low-level helpers (retained for binary compatibility with the header)
// -----------------------------------------------------------------------

DWORD RarArchive::ReadDWord(BYTE* buf, int off)
{
    DWORD v = 0;
    memcpy(&v, buf + off, 4);
    return v;
}

WORD RarArchive::ReadWord(BYTE* buf, int off)
{
    WORD v = 0;
    memcpy(&v, buf + off, 2);
    return v;
}

std::vector<BYTE> RarArchive::ReadBytes(DWORD offset, DWORD size)
{
    if (size == 0 || m_hFile == INVALID_HANDLE_VALUE) return {};
    std::vector<BYTE> buf(size);

    LARGE_INTEGER li;
    li.QuadPart = offset;
    if (!::SetFilePointerEx(m_hFile, li, NULL, FILE_BEGIN))
        return {};

    DWORD bytesRead = 0;
    if (!::ReadFile(m_hFile, buf.data(), size, &bytesRead, NULL) || bytesRead != size)
        return {};

    return buf;
}

void RarArchive::CleanupTempFiles()
{
    for (const auto& f : m_tempFiles)
        ::DeleteFileW(f.c_str());
    m_tempFiles.clear();
}
