# Test the tooltip COM object end-to-end
$clsid = "{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}"
$testFile = "C:\Users\Arturo\Downloads\TransferNow-20240625mvU08kuj\1\666 999\666 999.cbz"

if (-not (Test-Path $testFile)) {
    $found = Get-ChildItem "$env:USERPROFILE" -Recurse -Include "*.cbz","*.cbr" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $testFile = $found.FullName } else { Write-Host "No CBZ/CBR file found"; exit 1 }
}

Write-Host "Testing with: $testFile"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

[ComImport, Guid("BB2E617C-0920-11D1-9A0B-00C04FC2D6C1"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IExtractImage {
    void GetLocation([MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pszPathBuffer, int cch, ref int pdwPriority, ref SIZE prgSize, int dwRecClrDepth, ref int pdwFlags);
    void Extract(out IntPtr phBmpImage);
}

[ComImport, Guid("B7D14566-0509-4CCE-A71F-0A554233BD9B"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IInitializeWithFile {
    void Initialize([MarshalAs(UnmanagedType.LPWStr)] string pszFilePath, uint grfMode);
}

[ComImport, Guid("00021500-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IQueryInfo {
    void GetInfoFlags(out uint pdwFlags);
    void GetInfoTip(uint dwFlags, [MarshalAs(UnmanagedType.LPWStr)] out string ppwszTipText);
}

public struct SIZE { public int cx; public int cy; }
"@

try {
    $type = [Type]::GetTypeFromCLSID([Guid]$clsid.Trim('{}'))
    $obj = [Activator]::CreateInstance($type)
    Write-Host "COM object created OK"

    $initWithFile = [IInitializeWithFile]$obj
    $initWithFile.Initialize($testFile, 0)
    Write-Host "Initialize() called OK"

    $queryInfo = [IQueryInfo]$obj
    $tip = $null
    $flags = 0
    $queryInfo.GetInfoTip($flags, [ref]$tip)
    Write-Host "GetInfoTip() returned:"
    Write-Host "---"
    Write-Host $tip
    Write-Host "---"
} catch {
    Write-Host "ERROR: $($_.Exception.Message)"
    Write-Host $_.ScriptStackTrace
}
