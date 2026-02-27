// ZipArchive.cpp : CBZ file handling implementation

#include "pch.h"
#include "ZipArchive.h"

// ZIP file format constants
#define ZIP_LOCAL_FILE_HEADER_SIGNATURE  0x04034b50UL
#define ZIP_CENTRAL_DIRECTORY_SIGNATURE  0x02014b50UL
#define ZIP_END_CENTRAL_DIR_SIGNATURE    0x06054b50UL

ZipArchive::ZipArchive() : m_hFile(INVALID_HANDLE_VALUE) {}

ZipArchive::~ZipArchive() { Close(); }

bool ZipArchive::Open(const std::wstring& filePath)
{
    if (m_hFile != INVALID_HANDLE_VALUE) Close();

    m_hFile = ::CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) return false;

    m_filePath = filePath;
    return ParseCentralDirectory();
}

void ZipArchive::Close()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
    CleanupTempFiles();
    m_fileList.clear();
}

std::vector<ZipFileInfo> ZipArchive::GetFileList()
{
    return m_fileList;
}

std::vector<BYTE> ZipArchive::ExtractFile(const std::wstring& fileName)
{
    std::vector<BYTE> result;
    for (const auto& fi : m_fileList)
    {
        if (!fi.IsDirectory && fi.FileName == fileName)
        {
            ReadLocalFileHeader(fi, result);
            break;
        }
    }
    return result;
}

std::wstring ZipArchive::ExtractFileToTemp(const std::wstring& fileName)
{
    std::vector<BYTE> data = ExtractFile(fileName);
    if (data.empty()) return L"";

    wchar_t tmpDir[MAX_PATH], tmpFile[MAX_PATH];
    if (!::GetTempPathW(MAX_PATH, tmpDir))    return L"";
    if (!::GetTempFileNameW(tmpDir, L"cbz", 0, tmpFile)) return L"";

    // Re-open with the right extension so the shell can render it
    // Append the original extension to the temp filename
    std::wstring ext;
    size_t dot = fileName.rfind(L'.');
    if (dot != std::wstring::npos) ext = fileName.substr(dot);

    std::wstring finalPath = tmpFile;
    if (!ext.empty())
    {
        finalPath += ext;
        // Delete the placeholder created by GetTempFileName
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

std::wstring ZipArchive::FindFirstImageFile()
{
    static const wchar_t* kExts[] = { L".jpg", L".jpeg", L".png", L".gif", L".bmp", L".webp", nullptr };

    // Sort the file list copy so covers come first (alphabetical)
    std::vector<ZipFileInfo> sorted = m_fileList;
    std::sort(sorted.begin(), sorted.end(),
              [](const ZipFileInfo& a, const ZipFileInfo& b){ return a.FileName < b.FileName; });

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

bool ZipArchive::HasComicInfoXML()
{
    for (const auto& fi : m_fileList)
        if (_wcsicmp(fi.FileName.c_str(), L"ComicInfo.xml") == 0) return true;
    return false;
}

std::wstring ZipArchive::ExtractComicInfoXML()
{
    // Find case-insensitively
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

    // The file is UTF-8; convert to wstring
    int wlen = ::MultiByteToWideChar(CP_UTF8, 0,
                                     reinterpret_cast<const char*>(raw.data()),
                                     static_cast<int>(raw.size()),
                                     NULL, 0);
    if (wlen <= 0) return L"";
    std::wstring result(wlen, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0,
                          reinterpret_cast<const char*>(raw.data()),
                          static_cast<int>(raw.size()),
                          &result[0], wlen);
    return result;
}

// -----------------------------------------------------------------------

bool ZipArchive::ParseCentralDirectory()
{
    DWORD fileSize = ::GetFileSize(m_hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize < 22) return false;

    // Scan backwards for EOCD signature
    DWORD searchLen = (fileSize < 65557) ? fileSize : 65557;
    DWORD searchStart = fileSize - searchLen;

    std::vector<BYTE> tail = ReadBytes(searchStart, searchLen);
    if (tail.empty()) return false;

    for (int i = static_cast<int>(tail.size()) - 22; i >= 0; --i)
    {
        if (ReadDWord(tail.data(), i) == ZIP_END_CENTRAL_DIR_SIGNATURE)
        {
            DWORD cdSize   = ReadDWord(tail.data(), i + 12);
            DWORD cdOffset = ReadDWord(tail.data(), i + 16);
            return ParseCentralDirectoryEntries(cdOffset, cdSize);
        }
    }
    return false;
}

bool ZipArchive::ParseCentralDirectoryEntries(DWORD offset, DWORD size)
{
    if (size == 0) return false;
    std::vector<BYTE> cd = ReadBytes(offset, size);
    if (cd.size() < size) return false;

    DWORD pos = 0;
    while (pos + 46 <= size)
    {
        if (ReadDWord(cd.data(), pos) != ZIP_CENTRAL_DIRECTORY_SIGNATURE) break;

        ZipFileInfo fi = {};
        fi.CRC32          = ReadDWord(cd.data(), pos + 16);
        fi.CompressedSize = ReadDWord(cd.data(), pos + 20);
        fi.FileSize       = ReadDWord(cd.data(), pos + 24);

        WORD fnLen    = ReadWord(cd.data(), pos + 28);
        WORD extraLen = ReadWord(cd.data(), pos + 30);
        WORD cmtLen   = ReadWord(cd.data(), pos + 32);

        // External file attributes: bit 4 of high byte = directory (MS-DOS)
        DWORD extAttr = ReadDWord(cd.data(), pos + 38);
        fi.IsDirectory = ((extAttr >> 16) & 0x10) != 0;

        fi.FileOffset = ReadDWord(cd.data(), pos + 42);

        // Filename is stored as CP437 or UTF-8 (flag bit 11)
        WORD genFlags = ReadWord(cd.data(), pos + 8);
        bool isUtf8   = (genFlags & (1 << 11)) != 0;

        if (pos + 46 + fnLen <= size)
        {
            const char* pName = reinterpret_cast<const char*>(cd.data() + pos + 46);
            int cp = isUtf8 ? CP_UTF8 : CP_ACP;
            int wlen = ::MultiByteToWideChar(cp, 0, pName, fnLen, NULL, 0);
            if (wlen > 0)
            {
                fi.FileName.resize(wlen);
                ::MultiByteToWideChar(cp, 0, pName, fnLen, &fi.FileName[0], wlen);
            }
            // Also detect directory by trailing slash
            if (!fi.FileName.empty() && fi.FileName.back() == L'/')
                fi.IsDirectory = true;
        }

        m_fileList.push_back(fi);
        pos += 46 + fnLen + extraLen + cmtLen;
    }
    return !m_fileList.empty();
}

bool ZipArchive::ReadLocalFileHeader(const ZipFileInfo& fileInfo, std::vector<BYTE>& data)
{
    std::vector<BYTE> hdr = ReadBytes(fileInfo.FileOffset, 30);
    if (hdr.size() < 30) return false;
    if (ReadDWord(hdr.data(), 0) != ZIP_LOCAL_FILE_HEADER_SIGNATURE) return false;

    WORD fnLen    = ReadWord(hdr.data(), 26);
    WORD extraLen = ReadWord(hdr.data(), 28);
    DWORD dataOff = fileInfo.FileOffset + 30 + fnLen + extraLen;

    // Only store-method (0) supported here; compressed files get raw bytes
    data = ReadBytes(dataOff, fileInfo.CompressedSize);
    return !data.empty();
}

DWORD ZipArchive::ReadDWord(BYTE* buf, int off)
{
    DWORD v = 0;
    memcpy(&v, buf + off, 4);
    return v;
}

WORD ZipArchive::ReadWord(BYTE* buf, int off)
{
    WORD v = 0;
    memcpy(&v, buf + off, 2);
    return v;
}

std::vector<BYTE> ZipArchive::ReadBytes(DWORD offset, DWORD size)
{
    if (size == 0) return {};
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

void ZipArchive::CleanupTempFiles()
{
    for (const auto& f : m_tempFiles)
        ::DeleteFileW(f.c_str());
    m_tempFiles.clear();
}
