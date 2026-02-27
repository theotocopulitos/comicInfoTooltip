Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class PropTest {
    [DllImport("ole32.dll")]
    public static extern int CoInitialize(IntPtr pvReserved);
    [DllImport("ole32.dll")]
    public static extern void CoUninitialize();
    [DllImport("C:\\Windows\\System32\\ComicTooltipExt.dll", EntryPoint="DllGetClassObject")]
    public static extern int DllGetClassObject(ref Guid rclsid, ref Guid riid, out IntPtr ppv);
}
'@

$clsidBytes = [Guid]::Parse("A1B2C3D4-E5F6-7890-ABCD-EF1234567892")
$iclsfBytes  = [Guid]::Parse("00000001-0000-0000-C000-000000000046")  # IClassFactory

[PropTest]::CoInitialize([IntPtr]::Zero) | Out-Null
$ppv = [IntPtr]::Zero
$hr = [PropTest]::DllGetClassObject([ref]$clsidBytes, [ref]$iclsfBytes, [ref]$ppv)
Write-Host "DllGetClassObject HRESULT: 0x$($hr.ToString('X8'))"
Write-Host "ppv: $ppv"
[PropTest]::CoUninitialize()

Start-Sleep 1
if (Test-Path "C:\ComicTooltipExt_debug.log") {
    Write-Host "LOG EXISTS:"
    Get-Content "C:\ComicTooltipExt_debug.log"
} else {
    Write-Host "No log file - DLL was not called"
}
