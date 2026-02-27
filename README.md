# Comic Tooltip Shell Extension

A Windows shell extension that displays rich tooltips for CBR and CBZ comic files, showing the comic cover and metadata in a two-column layout.

## Features

- **Two-column tooltip layout**: Comic cover image on the left, metadata on the right
- **Supports CBR and CBZ file formats**
- **Extracts and displays ComicInfo.xml metadata** including:
  - Title, Series, Volume, Issue numbers
  - Publisher, Publication date, Genre
  - Writer, Penciller, Inker, Colorist, Letterer, Cover Artist
  - Story arc information, Characters
  - Summary and additional metadata
- **Fallback to basic file information** when metadata is unavailable
- **Clean HTML-based tooltips** with modern styling
- **Windows 10/11 compatible**

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 with C++ ATL support
- Administrator privileges for installation

## Building

1. Open `ComicTooltipExt.vcxproj` in Visual Studio 2022
2. Build the solution in Release or Debug configuration
3. The output will be `ComicTooltipExt.dll`

## Installation

1. Run `install.bat` as Administrator
2. Restart Windows Explorer to see the changes

## Uninstallation

1. Run `uninstall.bat` as Administrator
2. Restart Windows Explorer to see the changes

## Usage

After installation, simply hover your mouse over any .cbr or .cbz file in Windows Explorer to see the tooltip with comic information and cover image.

## Technical Details

### Architecture

- **COM Shell Extension**: Implements `IQueryInfo` interface for tooltip handling
- **ATL Framework**: Uses Active Template Library for COM infrastructure
- **File Parsers**: Custom parsers for ZIP (CBZ) and RAR (CBR) formats
- **XML Parser**: Simple ComicInfo.xml parser for metadata extraction
- **HTML Tooltips**: Generates HTML content for rich tooltip display

### File Structure

```
ComicTooltipExt.cpp/h    - Main COM class implementation
ComicInfo.cpp/h          - ComicInfo.xml parser
ZipArchive.cpp/h         - CBZ (ZIP) file handling
RarArchive.cpp/h         - CBR (RAR) file handling
dllmain.cpp              - DLL entry points and registration
pch.h/cpp                - Pre-compiled headers
*.idl, *.rc, *.rgs       - COM interface definitions and resources
install.bat/uninstall.bat - Installation scripts
```

### COM Registration

The extension registers itself for the tooltip handler GUID `{00021500-0000-0000-C000-000000000046}` for both `.cbr` and `.cbz` file extensions.

### Security Considerations

- The extension runs in the Windows Explorer process
- File access is read-only
- Temporary files are created for cover images and cleaned up automatically
- No network access or external dependencies

## Limitations

- RAR (CBR) support is simplified and assumes uncompressed files
- For production use, consider integrating a full RAR library like UnRAR
- Large comic files may have tooltip loading delays
- HTML rendering is limited to Windows Explorer's built-in HTML capabilities

## License

This project is provided as-is for educational and personal use.

## Contributing

Feel free to submit issues and enhancement requests!
