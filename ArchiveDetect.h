// ArchiveDetect.h : Detect archive format by magic bytes
//
// Reads the first bytes of a file to determine if it is a ZIP or RAR archive,
// regardless of the file extension.  This allows .cbr files that are actually
// ZIP archives (and vice-versa) to be handled correctly.

#pragma once
#include <windows.h>

enum class ArchiveFormat { Unknown, Zip, Rar };

inline ArchiveFormat DetectArchiveFormat(const wchar_t* filePath)
{
    HANDLE hFile = ::CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
                                 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return ArchiveFormat::Unknown;

    BYTE buf[8] = {};
    DWORD bytesRead = 0;
    ::ReadFile(hFile, buf, 8, &bytesRead, NULL);
    ::CloseHandle(hFile);

    if (bytesRead < 4) return ArchiveFormat::Unknown;

    // ZIP: PK\x03\x04
    if (buf[0] == 0x50 && buf[1] == 0x4B && buf[2] == 0x03 && buf[3] == 0x04)
        return ArchiveFormat::Zip;

    // RAR4: Rar!\x1A\x07\x00   RAR5: Rar!\x1A\x07\x01\x00
    if (bytesRead >= 7 &&
        buf[0] == 0x52 && buf[1] == 0x61 && buf[2] == 0x72 && buf[3] == 0x21 &&
        buf[4] == 0x1A && buf[5] == 0x07)
        return ArchiveFormat::Rar;

    return ArchiveFormat::Unknown;
}
