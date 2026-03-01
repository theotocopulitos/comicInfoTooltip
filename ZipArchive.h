// ZipArchive.h : CBZ file handling

#pragma once

struct ZipFileInfo
{
    std::wstring FileName;
    DWORD FileSize;        // uncompressed size
    DWORD CompressedSize;
    DWORD CRC32;
    DWORD FileOffset;
    WORD  CompressionMethod; // 0=store, 8=deflate
    bool IsDirectory;
};

class ZipArchive
{
public:
    ZipArchive();
    ~ZipArchive();

    // Open a ZIP archive
    bool Open(const std::wstring& filePath);
    
    // Close the archive
    void Close();
    
    // Check if archive is open
    bool IsOpen() const { return m_hFile != INVALID_HANDLE_VALUE; }
    
    // Get list of files in archive
    std::vector<ZipFileInfo> GetFileList();
    
    // Extract a file to memory
    std::vector<BYTE> ExtractFile(const std::wstring& fileName);
    
    // Extract a file to a temporary file
    std::wstring ExtractFileToTemp(const std::wstring& fileName);
    
    // Find first image file (for cover)
    std::wstring FindFirstImageFile();
    
    // Check if ComicInfo.xml exists
    bool HasComicInfoXML();
    
    // Extract ComicInfo.xml content
    std::wstring ExtractComicInfoXML();

private:
    HANDLE m_hFile;
    std::wstring m_filePath;
    
    // ZIP file structure parsing
    bool ParseCentralDirectory();
    bool ParseCentralDirectoryEntries(DWORD offset, DWORD size);
    bool ReadLocalFileHeader(const ZipFileInfo& fileInfo, std::vector<BYTE>& data);

    // Helper methods
    DWORD ReadDWord(BYTE* buffer, int offset);
    WORD  ReadWord (BYTE* buffer, int offset);
    std::vector<BYTE> ReadBytes(DWORD offset, DWORD size);
    
    // File list
    std::vector<ZipFileInfo> m_fileList;
    
    // Clean up temporary files
    void CleanupTempFiles();
    std::vector<std::wstring> m_tempFiles;
};
