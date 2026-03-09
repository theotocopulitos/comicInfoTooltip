# Comic Shell Extension for Windows

A Windows shell extension that adds rich **tooltips** and a **preview pane** for `.cbz` and `.cbr` comic archive files in Explorer.

---

## Quick Start

### Requirements

- Windows 10/11 (x64)
- The pre-built DLL (`x64\Release\ComicTooltipExt.dll`), or Visual Studio 2022 with C++ ATL to build from source.
- Administrator permissions to copy the DLL into `C:\Windows\System32` and register the shell extensions.

### Build

1. Open `ComicTooltipExt.vcxproj` in Visual Studio 2022.
2. Build the project in **Release | x64**.
3. The output DLL will be generated at `x64\Release\ComicTooltipExt.dll`.

### Installation

1. **Build** the project in `Release x64` or use the existing DLL from `x64\Release`.
2. **Deploy the DLL**:
   - For development, run `deploy.bat` as administrator. It stops `prevhost.exe`, `dllhost.exe`, and `explorer.exe`, copies the DLL to `C:\Windows\System32`, then restarts Explorer.
   - If you prefer, you can also copy the DLL manually to `C:\Windows\System32` using an elevated prompt.
3. **Register** by right-clicking one of the batch files below and selecting **"Run as administrator"**:

| Script | What it does |
|---|---|
| `register_all.bat` | Registers **both** tooltip and preview handlers |
| `register_tooltip.bat` | Registers the **tooltip** handler only |
| `register_preview.bat` | Registers the **preview pane** handler only |

> **Important:** All registration scripts must run in an **elevated prompt** (Run as administrator). They will refuse to run otherwise.

4. If you did not use `deploy.bat`, **restart Explorer** or log out/in for changes to take effect.

### Uninstallation

Right-click the corresponding script as administrator:

| Script | What it does |
|---|---|
| `unregister_all.bat` | Removes **both** handlers |
| `unregister_tooltip.bat` | Removes the **tooltip** handler only |
| `unregister_preview.bat` | Removes the **preview pane** handler only |

### Usage

- **Tooltip:** Hover over any `.cbz` or `.cbr` file in Explorer to see metadata (title, series, publisher, creative team, summary, etc.).
- **Preview Pane:** Press `Alt+P` in Explorer to open the preview pane, then click a comic file. The current page/cover is shown on the left and metadata on the right.
- **Page navigation:** In the preview pane you can move between pages with the navigation controls or keyboard shortcuts when available.

### Troubleshooting

| Problem | Solution |
|---|---|
| CBR shows blank pages / no preview | Obtain `unrar.dll` (64-bit) from [rarlab.com/rar/UnRARDLL.exe](https://www.rarlab.com/rar/UnRARDLL.exe) and place it in the same directory as `ComicTooltipExt.dll` (e.g. `C:\Windows\System32`). Without this DLL, CBR support is gracefully disabled. |
| Tooltip doesn't appear | Restart Explorer. Verify with `reg query "HKCR\.cbz\shellex\{00021500-0000-0000-C000-000000000046}"` |
| Preview shows "No preview available" | Run `register_preview.bat` as admin. Check that `AppID` and `DisableLowILProcessIsolation` are set (the script does this). |
| DLL can't be replaced / file locked | Close Explorer and kill `prevhost.exe` and `dllhost.exe` before copying. `deploy.bat` automates this for development installs. |
| Preview shows only the cover (from CDisplay/ComicRack) | The preview handler must also be registered under the ProgID. `register_preview.bat` handles this. |
| Build fails with `LNK1104` on `ComicTooltipExt.dll` | The output DLL is still loaded by Explorer or Preview Host. Run `deploy.bat` or close `explorer.exe`, `prevhost.exe`, and `dllhost.exe` before rebuilding. |

---

## Technical Reference

### Features

- **Tooltip handler** (`IQueryInfo`): Plain-text tooltip with all ComicInfo.xml v2.1 fields, formatted with Unicode separators and sections.
- **Preview handler** (`IPreviewHandler`): GDI+ rendered panel with image/page area (left) and metadata (right) in a light theme. Supports resizing and page navigation.
- **Archive support**: Custom ZIP parser for `.cbz` and UnRAR SDK (`unrar.dll`) for `.cbr`. ZIP supports store and deflate-compressed entries. RAR support covers both RAR4 and RAR5 formats via the RARLAB UnRAR DLL loaded dynamically at runtime. If `unrar.dll` is absent, CBR archives gracefully degrade to a filename/size tooltip.
- **Metadata**: Full ComicInfo.xml v2.1 schema support including series, publication, creative team, story arcs, characters, ratings, and more.

### Architecture

```text
ComicTooltipExt.dll
├── COM entry points ─────────── dllmain.cpp
├── Tooltip handler (IQueryInfo) ─ ComicTooltipExt.cpp/h
├── Preview handler (IPreviewHandler) ─ ComicPreviewHandler.cpp/h
├── Metadata parser ──────────── ComicInfo.cpp/h
├── CBZ archive reader ────────── ZipArchive.cpp/h
├── CBR archive reader ────────── RarArchive.cpp/h
├── Precompiled headers ───────── pch.h/cpp
└── Resources / IDL / DEF ─────── *.rc, *.idl, *.def, *.rgs
```

### COM Classes

| Class | CLSID | Interfaces |
|---|---|---|
| `CComicTooltipExt` | `{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}` | `IPersistFile`, `IQueryInfo` |
| `CComicPreviewHandler` | `{B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}` | `IPreviewHandler`, `IInitializeWithFile`, `IOleWindow`, `IObjectWithSite` |

### Shell Extension GUIDs

| Handler | Shell Extension GUID |
|---|---|
| Tooltip (`IQueryInfo`) | `{00021500-0000-0000-C000-000000000046}` |
| Preview (`IPreviewHandler`) | `{8895b1c6-b41f-4c1c-a562-0d564250836f}` |

### Registry Layout

The tooltip handler registers under:

```text
HKCR\.cbz\shellex\{00021500-...} = {A1B2C3D4-...}
HKCR\.cbr\shellex\{00021500-...} = {A1B2C3D4-...}
HKCR\CLSID\{A1B2C3D4-...}\InprocServer32 = <path to DLL>
```

The preview handler additionally requires:

```text
HKCR\CLSID\{B5E84A2F-...}\InprocServer32 = <path to DLL>
HKCR\CLSID\{B5E84A2F-...}\AppID = {6d2b5079-2f0b-48dd-ab7f-97cec514d30b}
HKCR\CLSID\{B5E84A2F-...}\DisableLowILProcessIsolation = 1 (DWORD)
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PreviewHandlers\{B5E84A2F-...} = "Comic Preview Handler"
HKCR\.cbz\shellex\{8895b1c6-...} = {B5E84A2F-...}
HKCR\.cbr\shellex\{8895b1c6-...} = {B5E84A2F-...}
```

The `AppID` points to the `prevhost.exe` surrogate (`{6d2b5079-...}`), which is required because Windows runs preview handlers out-of-process for security. `DisableLowILProcessIsolation` allows the handler to read files from the user's drives.

If a ProgID is registered for `.cbz`/`.cbr` (e.g. `cYo.ComicRack`), the preview handler must also be registered under that ProgID's `shellex` key.

### Build Dependencies

- **Visual Studio 2022** with C++ ATL/MFC workload
- **GDI+** (`gdiplus.lib`) — for preview handler rendering
- **Windows Compression API** (`Cabinet.lib`, `compressapi.h`) — for ZIP deflate decompression
- **UnRAR DLL** (runtime, not link-time) — place `unrar.dll` (64-bit) from [rarlab.com](https://www.rarlab.com/rar/UnRARDLL.exe) next to `ComicTooltipExt.dll` to enable CBR support

### Key Implementation Notes

- **ZIP directory detection**: The `versionMadeByOS` byte in the central directory determines how to interpret external attributes. Unix-origin ZIPs use `S_IFDIR` (0x4000) in the high 16 bits; MS-DOS uses bit 0x10 in the low byte. Trailing slash is always checked as a fallback.
- **DPI handling**: GDI+ page unit is set to `UnitPixel` and all fonts use `UnitPixel` to prevent DPI auto-scaling in RDP or high-DPI scenarios.
- **GDI+ Fonts**: The `Gdiplus::Font` class has a deleted copy constructor. All font parameters must be passed as pointers.
- **Double buffering**: The preview handler renders to an off-screen bitmap and blits to avoid flicker.

### Security

- Read-only file access; no writes except temporary files (cleaned up automatically).
- No network access; no compile-time external dependencies. CBR preview requires the RARLAB UnRAR DLL (`unrar.dll`) at runtime — it is loaded dynamically, and if absent CBRs degrade gracefully to filename/size tooltips.
- The preview handler runs in the `prevhost.exe` surrogate process, isolated from Explorer.

## License

This project is provided as-is for educational and personal use.
