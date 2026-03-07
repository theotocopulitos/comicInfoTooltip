// dll.hpp : UnRAR DLL API declarations
//
// Based on the official RARLAB UnRAR DLL SDK header.
// Structures must be packed to 1-byte boundaries for binary compatibility
// with unrar.dll.

#pragma once

// -----------------------------------------------------------------------
// Error codes returned by RAROpenArchiveEx / RARReadHeaderEx / RARProcessFileW
// -----------------------------------------------------------------------
#define ERAR_SUCCESS           0
#define ERAR_END_ARCHIVE      10
#define ERAR_NO_MEMORY        11
#define ERAR_BAD_DATA         12
#define ERAR_BAD_ARCHIVE      13
#define ERAR_UNKNOWN_FORMAT   14
#define ERAR_EOPEN            15
#define ERAR_ECREATE          16
#define ERAR_ECLOSE           17
#define ERAR_EREAD            18
#define ERAR_EWRITE           19
#define ERAR_SMALL_BUF        20
#define ERAR_UNKNOWN          21
#define ERAR_MISSING_PASSWORD 22
#define ERAR_EREFERENCE       23
#define ERAR_TOO_LARGE_FILE   24

// -----------------------------------------------------------------------
// Open modes (RAROpenArchiveDataEx::OpenMode)
// -----------------------------------------------------------------------
#define RAR_OM_LIST           0   // List files, do not extract
#define RAR_OM_EXTRACT        1   // Extract files
#define RAR_OM_LIST_INCSPLIT  2   // List files, include split parts

// -----------------------------------------------------------------------
// Process operations (RARProcessFile / RARProcessFileW Operation)
// -----------------------------------------------------------------------
#define RAR_SKIP              0   // Skip the current file
#define RAR_TEST              1   // Test (decompress to memory, invoke callback)
#define RAR_EXTRACT           2   // Extract to disk

// -----------------------------------------------------------------------
// Volume change modes (UCM_CHANGEVOLUME callback)
// -----------------------------------------------------------------------
#define RAR_VOL_ASK           0
#define RAR_VOL_NOTIFY        1

// -----------------------------------------------------------------------
// DLL version
// -----------------------------------------------------------------------
#define RAR_DLL_VERSION       8

// -----------------------------------------------------------------------
// Callback message IDs (UNRARCALLBACK)
// -----------------------------------------------------------------------
#define UCM_CHANGEVOLUME      0   // Change volume
#define UCM_PROCESSDATA       1   // Data decompressed (P1 = ptr, P2 = size)
#define UCM_NEEDPASSWORD      2   // Password needed
#define UCM_CHANGEVOLUMEW     3   // Change volume (Unicode)
#define UCM_NEEDPASSWORDW     4   // Password needed (Unicode)

// -----------------------------------------------------------------------
// Callback function type
// -----------------------------------------------------------------------
typedef int (CALLBACK *UNRARCALLBACK)(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2);

// -----------------------------------------------------------------------
// Structures — must match the binary layout of unrar.dll
// -----------------------------------------------------------------------
#pragma pack(1)

struct RARHeaderDataEx
{
    char         ArcName[1024];
    wchar_t      ArcNameW[1024];
    char         FileName[1024];
    wchar_t      FileNameW[1024];
    unsigned int Flags;
    unsigned int PackSize;
    unsigned int PackSizeHigh;
    unsigned int UnpSize;
    unsigned int UnpSizeHigh;
    unsigned int HostOS;
    unsigned int FileCRC;
    unsigned int FileTime;
    unsigned int UnpVer;
    unsigned int Method;
    unsigned int FileAttr;
    char        *CmtBuf;
    unsigned int CmtBufSize;
    unsigned int CmtSize;
    unsigned int CmtState;
    unsigned int DictSize;
    unsigned int HashType;
    char         Hash[32];
    unsigned int RedirType;
    wchar_t     *RedirName;
    unsigned int RedirNameSize;
    unsigned int DirTarget;
    unsigned int MtimeLow;
    unsigned int MtimeHigh;
    unsigned int AtimeLow;
    unsigned int AtimeHigh;
    unsigned int CtimeLow;
    unsigned int CtimeHigh;
    unsigned int Reserved[988];
};

struct RAROpenArchiveDataEx
{
    char         *ArcName;
    wchar_t      *ArcNameW;
    unsigned int  OpenMode;
    unsigned int  OpenResult;
    char         *CmtBuf;
    unsigned int  CmtBufSize;
    unsigned int  CmtSize;
    unsigned int  CmtState;
    unsigned int  Flags;
    UNRARCALLBACK Callback;
    LPARAM        UserData;
    unsigned int  OpFlags;
    wchar_t      *CmtBufW;
    unsigned int  Reserved[25];
};

#pragma pack()

// -----------------------------------------------------------------------
// Function pointer typedefs (all __stdcall / PASCAL)
// -----------------------------------------------------------------------
typedef HANDLE (PASCAL *PFN_RAROpenArchiveEx)(RAROpenArchiveDataEx *ArchiveData);
typedef int    (PASCAL *PFN_RARCloseArchive )(HANDLE hArcData);
typedef int    (PASCAL *PFN_RARReadHeaderEx )(HANDLE hArcData, RARHeaderDataEx *HeaderData);
typedef int    (PASCAL *PFN_RARProcessFileW )(HANDLE hArcData, int Operation,
                                              wchar_t *DestPath, wchar_t *DestName);
typedef void   (PASCAL *PFN_RARSetCallback  )(HANDLE hArcData, UNRARCALLBACK Callback,
                                              LPARAM UserData);
typedef int    (PASCAL *PFN_RARGetDllVersion)();
