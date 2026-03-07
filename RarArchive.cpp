// RarArchive.cpp : CBR (RAR) file handling implementation
//
// Uses the UnRAR DLL (unrar.dll) loaded dynamically at runtime to support
// both RAR4 and RAR5 archives.  If unrar.dll is absent, RAR support is not
// available and attempts to open RAR archives will fail accordingly.

#include "pch.h"
#include "RarArchive.h"
#include "dll.hpp"
#include <mutex>

// -----------------------------------------------------------------------
// Dynamic UnRAR DLL loader
// -----------------------------------------------------------------------

// Forward declaration — defined after g_unrar so Load() can take its address
// to resolve the owning DLL's path via GetModuleHandleEx.
static int CALLBACK UnRarCallback(UINT msg, LPARAM userData, LPARAM p1, LPARAM p2);

// Safety caps for archive-controlled allocation sizes.
// Both limits are 64 MB: a shell-extension preview handles one image at a time,
// so the per-entry cap and the per-extraction cap are intentionally the same.
static const size_t MAX_ENTRY_BYTES = 64 * 1024 * 1024;   // 64 MB per file
static const size_t MAX_TOTAL_BYTES = 64 * 1024 * 1024;   // 64 MB per extraction

struct UnRarDll
{
    HMODULE              hDll            = NULL;
    PFN_RAROpenArchiveEx pfnOpenArchiveEx = nullptr;
    PFN_RARCloseArchive  pfnCloseArchive  = nullptr;
    PFN_RARReadHeaderEx  pfnReadHeaderEx  = nullptr;
    PFN_RARProcessFileW  pfnProcessFileW  = nullptr;
    PFN_RARSetCallback   pfnSetCallback   = nullptr;
    std::once_flag       initOnce;
    bool                 initResult       = false;

    bool Load()
    {
        // Guarantee that DoLoad() is executed by exactly one thread; all
        // subsequent callers (including concurrent ones) block until the first
        // completes and then read the cached initResult.
        std::call_once(initOnce, [this]() { initResult = DoLoad(); });
        return initResult;
    }

    ~UnRarDll()
    {
        if (hDll) { ::FreeLibrary(hDll); hDll = NULL; }
    }

private:
    bool DoLoad()
    {
        // Build a full path to unrar.dll placed alongside ComicTooltipExt.dll
        // so we never accidentally load an unrar.dll from an arbitrary location
        // on the process DLL search path.
        wchar_t dllPath[MAX_PATH] = {};
        HMODULE hSelf = NULL;
        if (::GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(UnRarCallback),   // address inside this DLL
                &hSelf) && hSelf)
        {
            DWORD len = ::GetModuleFileNameW(hSelf, dllPath, MAX_PATH);
            if (len > 0 && len < MAX_PATH)
            {
                wchar_t* lastSlash = wcsrchr(dllPath, L'\\');
                if (lastSlash)
                {
                    // Replace filename portion with unrar.dll.
                    // Guard against arithmetic underflow before computing remaining.
                    ptrdiff_t offset = lastSlash + 1 - dllPath;
                    if (offset > 0 && static_cast<size_t>(offset) <= MAX_PATH)
                    {
                        size_t remaining = MAX_PATH - static_cast<size_t>(offset);
                        if (wcscpy_s(lastSlash + 1, remaining, L"unrar.dll") != 0)
                            dllPath[0] = L'\0';  // on failure fall back to bare name
                    }
                }
            }
        }

        // If we failed to construct a fully qualified path to unrar.dll,
        // fail closed rather than falling back to the default DLL search path.
        if (!dllPath[0])
            return false;

        HMODULE tmpH = ::LoadLibraryW(dllPath);
        if (!tmpH) return false;

        PFN_RAROpenArchiveEx tmpOpenArchiveEx = reinterpret_cast<PFN_RAROpenArchiveEx>(
            ::GetProcAddress(tmpH, "RAROpenArchiveEx"));
        PFN_RARCloseArchive  tmpCloseArchive  = reinterpret_cast<PFN_RARCloseArchive>(
            ::GetProcAddress(tmpH, "RARCloseArchive"));
        PFN_RARReadHeaderEx  tmpReadHeaderEx  = reinterpret_cast<PFN_RARReadHeaderEx>(
            ::GetProcAddress(tmpH, "RARReadHeaderEx"));
        PFN_RARProcessFileW  tmpProcessFileW  = reinterpret_cast<PFN_RARProcessFileW>(
            ::GetProcAddress(tmpH, "RARProcessFileW"));
        PFN_RARSetCallback   tmpSetCallback   = reinterpret_cast<PFN_RARSetCallback>(
            ::GetProcAddress(tmpH, "RARSetCallback"));

        if (!tmpOpenArchiveEx || !tmpCloseArchive || !tmpReadHeaderEx ||
            !tmpProcessFileW  || !tmpSetCallback)
        {
            ::FreeLibrary(tmpH);
            return false;
        }

        // All pointers resolved — publish atomically: hDll becomes non-null
        // only after every pfn* member is valid, so other threads can never
        // observe a non-null hDll with uninitialized function pointers.
        pfnOpenArchiveEx = tmpOpenArchiveEx;
        pfnCloseArchive  = tmpCloseArchive;
        pfnReadHeaderEx  = tmpReadHeaderEx;
        pfnProcessFileW  = tmpProcessFileW;
        pfnSetCallback   = tmpSetCallback;
        hDll             = tmpH;
        return true;
    }
};

static UnRarDll g_unrar;

// -----------------------------------------------------------------------
// UCM_PROCESSDATA callback — accumulates decompressed bytes
// -----------------------------------------------------------------------

struct ExtractionContext
{
    std::vector<BYTE>* pData        = nullptr;
    size_t             currentSize  = 0;
    bool               limitReached = false;
};

static int CALLBACK UnRarCallback(UINT msg, LPARAM userData, LPARAM p1, LPARAM p2)
{
    if (msg == UCM_PROCESSDATA)
    {
        ExtractionContext* ctx = reinterpret_cast<ExtractionContext*>(userData);
        if (ctx && ctx->pData && !ctx->limitReached)
        {
            const BYTE* ptr  = reinterpret_cast<const BYTE*>(p1);
            size_t      size = static_cast<size_t>(p2);
            if (ctx->currentSize + size > MAX_TOTAL_BYTES)
            {
                // Refuse oversized data and signal an error to UnRAR so the
                // extraction is aborted rather than filling memory unboundedly.
                ctx->limitReached = true;
                return -1;
            }
            ctx->pData->insert(ctx->pData->end(), ptr, ptr + size);
            ctx->currentSize += size;
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
            // Allocate room for the NUL that MultiByteToWideChar writes when
            // the source length is -1, then strip it afterwards.
            name.resize(wlen);
            ::MultiByteToWideChar(CP_ACP, 0, hdr.FileName, -1, &name[0], wlen);
            if (!name.empty() && name.back() == L'\0')
                name.pop_back();
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
        // Combine the high and low dwords into a full 64-bit uncompressed size
        // so the MAX_ENTRY_BYTES cap cannot be bypassed by a crafted archive
        // header that sets UnpSizeHigh != 0 while keeping UnpSize small.
        uint64_t unpSize64  = (static_cast<uint64_t>(hdr.UnpSizeHigh) << 32) | hdr.UnpSize;
        uint64_t packSize64 = (static_cast<uint64_t>(hdr.PackSizeHigh) << 32) | hdr.PackSize;

        // Skip entries whose true uncompressed size exceeds the safety cap;
        // they will not appear in the file list and cannot be extracted.
        if (unpSize64 > MAX_ENTRY_BYTES)
        {
            int skipRet = g_unrar.pfnProcessFileW(hArc, RAR_SKIP, NULL, NULL);
            if (skipRet != ERAR_SUCCESS)
                break;
            continue;
        }

        RarFileInfo fi   = {};
        fi.FileName      = HeaderFileName(hdr);
        // unpSize64 is guaranteed ≤ MAX_ENTRY_BYTES (≤ 64 MB) here, so
        // storing the low 32 bits in the DWORD field is safe.
        fi.FileSize       = static_cast<DWORD>(unpSize64);
        fi.CompressedSize = static_cast<DWORD>(packSize64);
        fi.CRC32          = hdr.FileCRC;
        fi.FileOffset     = 0;               // not used with UnRAR SDK
        fi.IsDirectory    = (hdr.FileAttr & 0x10) != 0;

        m_fileList.push_back(fi);

        // Must call ProcessFileW to advance the iterator; RAR_SKIP is free.
        // Abort if the SDK reports an error advancing past the entry.
        int skipRet = g_unrar.pfnProcessFileW(hArc, RAR_SKIP, NULL, NULL);
        if (skipRet != ERAR_SUCCESS)
            break;
    }

    g_unrar.pfnCloseArchive(hArc);

    // ERAR_END_ARCHIVE is the expected loop exit; any other code signals a
    // corrupt or unreadable archive — discard whatever was collected.
    if (ret != ERAR_END_ARCHIVE)
    {
        m_fileList.clear();
        return false;
    }
    // A valid archive that happens to be empty is not an error.
    return true;
}

// -----------------------------------------------------------------------
// UnRAR SDK — file extraction (callback accumulates decompressed data)
// -----------------------------------------------------------------------

bool RarArchive::ReadRarFileHeader(const RarFileInfo& fileInfo, std::vector<BYTE>& data)
{
    if (!g_unrar.Load()) return false;

    ExtractionContext ctx;
    ctx.pData = &data;
    // Refuse to even attempt extracting entries that exceed the safety cap —
    // the archive header is attacker-controlled and cannot be trusted.
    if (static_cast<size_t>(fileInfo.FileSize) > MAX_ENTRY_BYTES)
        return false;
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
    bool extractOk = false;
    RARHeaderDataEx hdr = {};
    int ret;
    while ((ret = g_unrar.pfnReadHeaderEx(hArc, &hdr)) == ERAR_SUCCESS)
    {
        std::wstring name = HeaderFileName(hdr);
        // Case-insensitive match to be consistent with ExtractFile() above.
        if (_wcsicmp(name.c_str(), fileInfo.FileName.c_str()) == 0)
        {
            // RAR_TEST decompresses into the callback; nothing is written to disk.
            // Key success off the SDK return code, not data.empty(), so that a
            // legitimately zero-byte entry is handled correctly.
            int procRet = g_unrar.pfnProcessFileW(hArc, RAR_TEST, NULL, NULL);
            found     = true;
            extractOk = (procRet == ERAR_SUCCESS);
            break;
        }
        int skipRet = g_unrar.pfnProcessFileW(hArc, RAR_SKIP, NULL, NULL);
        if (skipRet != ERAR_SUCCESS)
            break;
    }

    g_unrar.pfnCloseArchive(hArc);
    return found && extractOk;
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
