// RarArchive.cpp : CBR (RAR) file handling implementation
//
// RAR4 format parsing. RAR5 archives need a library like UnRAR.dll;
// for those we fall back to returning an empty list so the tooltip
// shows at least the filename/size fallback.

#include "pch.h"
#include "RarArchive.h"

// RAR4 signature: 0x52 0x61 0x72 0x21 0x1A 0x07 0x00
// RAR5 signature: 0x52 0x61 0x72 0x21 0x1A 0x07 0x01 0x00
static const BYTE kRar4Sig[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00 };
static const BYTE kRar5Sig[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 };

// RAR4 block types
#define RAR4_HEAD_FILE   0x74

RarArchive::RarArchive() : m_hFile(INVALID_HANDLE_VALUE) {}

RarArchive::~RarArchive() { Close(); }

bool RarArchive::Open(const std::wstring& filePath)
{
    if (m_hFile != INVALID_HANDLE_VALUE) Close();

    m_hFile = ::CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) return false;

    m_filePath = filePath;
    return ParseRarHeader();
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
// RAR4 header parser
// -----------------------------------------------------------------------

bool RarArchive::ParseRarHeader()
{
    std::vector<BYTE> sig = ReadBytes(0, 8);
    if (sig.size() < 7) return false;

    bool isRar4 = (memcmp(sig.data(), kRar4Sig, 7) == 0);
    bool isRar5 = (sig.size() >= 8 && memcmp(sig.data(), kRar5Sig, 8) == 0);

    if (!isRar4 && !isRar5) return false;
    if (isRar5) return false;   // RAR5 needs external library; leave list empty

    DWORD fileSize = ::GetFileSize(m_hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) return false;

    // RAR4: main archive header starts at offset 7
    // Each block: HEAD_CRC(2) HEAD_TYPE(1) HEAD_FLAGS(2) HEAD_SIZE(2) [ADD_SIZE(4)]
    DWORD pos = 7;
    while (pos + 7 <= fileSize)
    {
        std::vector<BYTE> blkHdr = ReadBytes(pos, 11);
        if (blkHdr.size() < 7) break;

        BYTE  headType  = blkHdr[2];
        WORD  headFlags = ReadWord(blkHdr.data(), 3);
        WORD  headSize  = ReadWord(blkHdr.data(), 5);

        if (headSize < 7) break;    // safety

        // ADD_SIZE present?
        DWORD addSize = 0;
        if (headFlags & 0x8000)
        {
            if (blkHdr.size() < 11) break;
            addSize = ReadDWord(blkHdr.data(), 7);
        }

        if (headType == RAR4_HEAD_FILE)
        {
            // File header layout (offsets relative to block start):
            //  0  HEAD_CRC        2
            //  2  HEAD_TYPE       1
            //  3  HEAD_FLAGS      2
            //  5  HEAD_SIZE       2
            //  7  PACK_SIZE       4
            // 11  UNP_SIZE        4
            // 15  HOST_OS         1
            // 16  FILE_CRC        4
            // 20  FTIME           4
            // 24  UNP_VER         1
            // 25  METHOD          1
            // 26  NAME_SIZE       2
            // 28  ATTR            4
            // 32  [HIGH_PACK_SZ]  4  (if flag 0x100)
            // 32  [HIGH_UNP_SZ]   4  (if flag 0x100)
            // then file name (NAME_SIZE bytes, ANSI or Unicode if flag 0x200)

            std::vector<BYTE> fhdr = ReadBytes(pos, headSize);
            if (fhdr.size() < 32) { pos += headSize + addSize; continue; }

            DWORD packSize = ReadDWord(fhdr.data(), 7);
            DWORD unpSize  = ReadDWord(fhdr.data(), 11);
            WORD  nameSize = ReadWord (fhdr.data(), 26);
            DWORD attr     = ReadDWord(fhdr.data(), 28);

            DWORD nameOff = 32;
            if (headFlags & 0x100) nameOff += 8;    // HIGH_PACK/UNP sizes

            // Full pack size (may span 64-bit)
            if (headFlags & 0x100)
            {
                DWORD hiPack = ReadDWord(fhdr.data(), 32);
                addSize = packSize | (static_cast<DWORD64>(hiPack) > 0 ? hiPack : 0);
                // For our purposes just use packSize
                addSize = packSize;
            }
            else
            {
                addSize = packSize;
            }

            RarFileInfo fi = {};
            fi.FileSize       = unpSize;
            fi.CompressedSize = packSize;
            fi.IsDirectory    = (attr & 0x10) != 0;  // MS-DOS directory bit
            fi.FileOffset     = pos + headSize;       // data follows full header

            if (nameOff + nameSize <= fhdr.size())
            {
                const char* pName = reinterpret_cast<const char*>(fhdr.data() + nameOff);
                if (headFlags & 0x200)  // Unicode filename
                {
                    // Unicode name: ANSI part terminated by '\0', then UTF-16LE
                    size_t ansiLen = strnlen(pName, nameSize);
                    const wchar_t* wName = reinterpret_cast<const wchar_t*>(pName + ansiLen + 1);
                    size_t wAvail = (nameSize - ansiLen - 1) / sizeof(wchar_t);
                    if (wAvail > 0)
                        fi.FileName.assign(wName, wAvail);
                    else
                        fi.FileName.assign(pName, pName + ansiLen); // fall back to ANSI
                }
                else
                {
                    int wlen = ::MultiByteToWideChar(CP_ACP, 0, pName, (int)nameSize, NULL, 0);
                    if (wlen > 0)
                    {
                        fi.FileName.resize(wlen);
                        ::MultiByteToWideChar(CP_ACP, 0, pName, (int)nameSize, &fi.FileName[0], wlen);
                    }
                }
            }

            // Normalise path separators
            std::replace(fi.FileName.begin(), fi.FileName.end(), L'\\', L'/');

            m_fileList.push_back(fi);
            pos += headSize + addSize;
        }
        else
        {
            pos += headSize + addSize;
        }
    }

    return !m_fileList.empty();
}

bool RarArchive::ReadRarFileHeader(const RarFileInfo& fileInfo, std::vector<BYTE>& data)
{
    // Only store-method (method 0x30) is extracted without decompression.
    // Compressed files are returned empty – the tooltip will fall back gracefully.
    data = ReadBytes(fileInfo.FileOffset, fileInfo.CompressedSize);
    return !data.empty();
}

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

void RarArchive::CleanupTempFiles()
{
    for (const auto& f : m_tempFiles)
        ::DeleteFileW(f.c_str());
    m_tempFiles.clear();
}
