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

static bool IsComicInfoEntryName(const std::wstring& fileName)
{
    size_t slash = fileName.find_last_of(L"/\\");
    const wchar_t* baseName = (slash == std::wstring::npos)
        ? fileName.c_str()
        : fileName.c_str() + slash + 1;
    return _wcsicmp(baseName, L"ComicInfo.xml") == 0;
}

static std::wstring DecodeXmlText(const std::vector<BYTE>& raw)
{
    if (raw.empty()) return L"";

    if (raw.size() >= 2)
    {
        if (raw[0] == 0xFF && raw[1] == 0xFE)
        {
            size_t wcharCount = (raw.size() - 2) / sizeof(wchar_t);
            return std::wstring(reinterpret_cast<const wchar_t*>(raw.data() + 2), wcharCount);
        }
        if (raw[0] == 0xFE && raw[1] == 0xFF)
        {
            std::wstring out;
            for (size_t i = 2; i + 1 < raw.size(); i += 2)
            {
                wchar_t ch = static_cast<wchar_t>((raw[i] << 8) | raw[i + 1]);
                out.push_back(ch);
            }
            return out;
        }
    }

    int utf8Skip = 0;
    if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF)
        utf8Skip = 3;

    int wlen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                      reinterpret_cast<const char*>(raw.data() + utf8Skip),
                                      static_cast<int>(raw.size() - utf8Skip),
                                      NULL, 0);
    if (wlen > 0)
    {
        std::wstring result(wlen, L'\0');
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                              reinterpret_cast<const char*>(raw.data() + utf8Skip),
                              static_cast<int>(raw.size() - utf8Skip),
                              &result[0], wlen);
        return result;
    }

    wlen = ::MultiByteToWideChar(CP_ACP, 0,
                                  reinterpret_cast<const char*>(raw.data()),
                                  static_cast<int>(raw.size()),
                                  NULL, 0);
    if (wlen <= 0) return L"";

    std::wstring result(wlen, L'\0');
    ::MultiByteToWideChar(CP_ACP, 0,
                           reinterpret_cast<const char*>(raw.data()),
                           static_cast<int>(raw.size()),
                           &result[0], wlen);
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
        if (IsComicInfoEntryName(fi.FileName)) return true;
    return false;
}

std::wstring ZipArchive::ExtractComicInfoXML()
{
    // Find case-insensitively
    std::wstring entryName;
    for (const auto& fi : m_fileList)
    {
        if (IsComicInfoEntryName(fi.FileName))
        {
            entryName = fi.FileName;
            break;
        }
    }
    if (entryName.empty()) return L"";

    std::vector<BYTE> raw = ExtractFile(entryName);
    if (raw.empty()) return L"";

    return DecodeXmlText(raw);
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

        fi.CompressionMethod = ReadWord(cd.data(), pos + 10);

        WORD fnLen    = ReadWord(cd.data(), pos + 28);
        WORD extraLen = ReadWord(cd.data(), pos + 30);
        WORD cmtLen   = ReadWord(cd.data(), pos + 32);

        // Determine directory status from external attributes
        DWORD extAttr = ReadDWord(cd.data(), pos + 38);
        BYTE versionMadeByOS = cd[pos + 5]; // high byte of "version made by"
        fi.IsDirectory = false;
        if (versionMadeByOS == 0) // MS-DOS / FAT
            fi.IsDirectory = (extAttr & 0x10) != 0;
        else if (versionMadeByOS == 3) // Unix
            fi.IsDirectory = ((extAttr >> 16) & 0x4000) != 0; // S_IFDIR
        // FileSize == 0 is also a hint but not conclusive

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
            // Trailing slash is the most reliable directory indicator
            if (!fi.FileName.empty() && fi.FileName.back() == L'/')
                fi.IsDirectory = true;
        }

        m_fileList.push_back(fi);
        pos += 46 + fnLen + extraLen + cmtLen;
    }
    return !m_fileList.empty();
}

// -----------------------------------------------------------------------
// Raw Inflate (RFC 1951) for ZIP deflate method 8
// -----------------------------------------------------------------------

namespace {
namespace inflate {

struct BitReader
{
    const BYTE* src;
    size_t      srcLen;
    size_t      bytePos;
    unsigned    bitBuf;
    int         bitCount;

    BitReader(const BYTE* d, size_t len)
        : src(d), srcLen(len), bytePos(0), bitBuf(0), bitCount(0) {}

    bool Ensure(int n)
    {
        while (bitCount < n)
        {
            if (bytePos >= srcLen) return false;
            bitBuf |= (unsigned)src[bytePos++] << bitCount;
            bitCount += 8;
        }
        return true;
    }

    unsigned Read(int n)
    {
        if (n == 0) return 0;
        if (!Ensure(n)) return 0;
        unsigned val = bitBuf & ((1u << n) - 1);
        bitBuf >>= n;
        bitCount -= n;
        return val;
    }

    void Align()
    {
        int skip = bitCount & 7;
        bitBuf >>= skip;
        bitCount -= skip;
    }
};

struct HuffTable
{
    unsigned short counts[16];
    unsigned short symbols[320];
    int maxLen;
};

static bool Build(HuffTable& ht, const unsigned short* lens, int n)
{
    memset(&ht, 0, sizeof(ht));
    for (int i = 0; i < n; i++)
    {
        if (lens[i] > 15) return false;
        ht.counts[lens[i]]++;
    }
    ht.counts[0] = 0;
    ht.maxLen = 0;
    for (int i = 15; i >= 1; i--)
        if (ht.counts[i]) { ht.maxLen = i; break; }

    unsigned short offs[16] = {};
    for (int i = 1; i < 16; i++)
        offs[i] = offs[i - 1] + ht.counts[i - 1];

    for (int i = 0; i < n; i++)
        if (lens[i])
            ht.symbols[offs[lens[i]]++] = (unsigned short)i;

    return true;
}

static int Decode(BitReader& br, const HuffTable& ht)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= ht.maxLen; len++)
    {
        if (!br.Ensure(1)) return -1;
        code = (code << 1) | (br.bitBuf & 1);
        br.bitBuf >>= 1;
        br.bitCount--;
        int count = ht.counts[len];
        if (code - first < count)
            return ht.symbols[index + (code - first)];
        index += count;
        first = (first + count) << 1;
    }
    return -1;
}

static const unsigned short kLenBase[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const unsigned short kLenExtra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const unsigned short kDistBase[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const unsigned short kDistExtra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};
static const int kCLOrder[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

static bool Run(const BYTE* src, size_t srcLen, std::vector<BYTE>& out, size_t hint)
{
    BitReader br(src, srcLen);
    out.clear();
    out.reserve(hint > 0 ? hint : srcLen * 4);

    unsigned bfinal;
    do
    {
        bfinal = br.Read(1);
        unsigned btype = br.Read(2);

        if (btype == 0)
        {
            // Stored block
            br.Align();
            unsigned len  = br.Read(16);
            /*nlen*/         br.Read(16);
            // Flush bit buffer before raw byte reads
            br.bytePos -= br.bitCount / 8;
            br.bitBuf = 0;
            br.bitCount = 0;
            for (unsigned i = 0; i < len; i++)
            {
                if (br.bytePos >= br.srcLen) return false;
                out.push_back(br.src[br.bytePos++]);
            }
        }
        else if (btype == 1 || btype == 2)
        {
            HuffTable litT, distT;

            if (btype == 1)
            {
                unsigned short ll[288];
                for (int i = 0;   i <= 143; i++) ll[i] = 8;
                for (int i = 144; i <= 255; i++) ll[i] = 9;
                for (int i = 256; i <= 279; i++) ll[i] = 7;
                for (int i = 280; i <= 287; i++) ll[i] = 8;
                Build(litT, ll, 288);

                unsigned short dl[32];
                for (int i = 0; i < 32; i++) dl[i] = 5;
                Build(distT, dl, 32);
            }
            else
            {
                unsigned hlit  = br.Read(5) + 257;
                unsigned hdist = br.Read(5) + 1;
                unsigned hclen = br.Read(4) + 4;

                unsigned short clLens[19] = {};
                for (unsigned i = 0; i < hclen; i++)
                    clLens[kCLOrder[i]] = (unsigned short)br.Read(3);

                HuffTable clT;
                if (!Build(clT, clLens, 19)) return false;

                unsigned total = hlit + hdist;
                unsigned short lengths[320] = {};
                unsigned idx = 0;
                while (idx < total)
                {
                    int sym = Decode(br, clT);
                    if (sym < 0) return false;
                    if (sym < 16)
                    {
                        lengths[idx++] = (unsigned short)sym;
                    }
                    else if (sym == 16)
                    {
                        unsigned rep = br.Read(2) + 3;
                        unsigned short prev = (idx > 0) ? lengths[idx - 1] : 0;
                        for (unsigned r = 0; r < rep && idx < total; r++)
                            lengths[idx++] = prev;
                    }
                    else if (sym == 17)
                    {
                        unsigned rep = br.Read(3) + 3;
                        for (unsigned r = 0; r < rep && idx < total; r++)
                            lengths[idx++] = 0;
                    }
                    else if (sym == 18)
                    {
                        unsigned rep = br.Read(7) + 11;
                        for (unsigned r = 0; r < rep && idx < total; r++)
                            lengths[idx++] = 0;
                    }
                    else return false;
                }

                if (!Build(litT, lengths, hlit)) return false;
                if (!Build(distT, lengths + hlit, hdist)) return false;
            }

            // Decode compressed data
            for (;;)
            {
                int sym = Decode(br, litT);
                if (sym < 0) return false;

                if (sym < 256)
                {
                    out.push_back((BYTE)sym);
                }
                else if (sym == 256)
                {
                    break;
                }
                else
                {
                    int li = sym - 257;
                    if (li < 0 || li >= 29) return false;
                    unsigned length = kLenBase[li] + br.Read(kLenExtra[li]);

                    int di = Decode(br, distT);
                    if (di < 0 || di >= 30) return false;
                    unsigned dist = kDistBase[di] + br.Read(kDistExtra[di]);

                    if (dist > out.size()) return false;
                    size_t srcPos = out.size() - dist;
                    for (unsigned i = 0; i < length; i++)
                    {
                        BYTE b = out[srcPos + i];
                        out.push_back(b);
                    }
                }
            }
        }
        else
        {
            return false;
        }
    } while (!bfinal);

    return true;
}

} // namespace inflate
} // anonymous namespace

// -----------------------------------------------------------------------

bool ZipArchive::ReadLocalFileHeader(const ZipFileInfo& fileInfo, std::vector<BYTE>& data)
{
    std::vector<BYTE> hdr = ReadBytes(fileInfo.FileOffset, 30);
    if (hdr.size() < 30) return false;
    if (ReadDWord(hdr.data(), 0) != ZIP_LOCAL_FILE_HEADER_SIGNATURE) return false;

    WORD fnLen    = ReadWord(hdr.data(), 26);
    WORD extraLen = ReadWord(hdr.data(), 28);
    DWORD dataOff = fileInfo.FileOffset + 30 + fnLen + extraLen;

    WORD localMethod = ReadWord(hdr.data(), 8);

    std::vector<BYTE> raw = ReadBytes(dataOff, fileInfo.CompressedSize);
    if (raw.empty()) return false;

    if (localMethod == 0)
    {
        // Store method: data is uncompressed
        data = std::move(raw);
    }
    else if (localMethod == 8)
    {
        // Deflate: use our RFC 1951 raw inflate implementation
        if (!inflate::Run(raw.data(), raw.size(), data, fileInfo.FileSize))
        {
            // Inflate failed — return empty
            data.clear();
            return false;
        }
    }
    else
    {
        // Unsupported compression method
        data.clear();
        return false;
    }
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
