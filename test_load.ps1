$dll = "C:\Users\Arturo\Documents\Windsurf\git\ComicsShellExtension\CascadeProjects\windsurf-project\x64\Release\ComicTooltipExt.dll"

$src = @"
using System;
using System.Runtime.InteropServices;
public class DllTest {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);
    [DllImport("kernel32.dll")]
    public static extern uint GetLastError();
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
    public static extern int FormatMessage(uint dwFlags, IntPtr lpSource, uint dwMessageId, uint dwLanguageId, System.Text.StringBuilder lpBuffer, uint nSize, IntPtr Arguments);
}
"@
Add-Type -TypeDefinition $src

$handle = [DllTest]::LoadLibraryEx($dll, [IntPtr]::Zero, 0)
if ($handle -eq [IntPtr]::Zero) {
    $err = [DllTest]::GetLastError()
    $sb = New-Object System.Text.StringBuilder 512
    [DllTest]::FormatMessage(0x1000, [IntPtr]::Zero, $err, 0, $sb, 512, [IntPtr]::Zero) | Out-Null
    Write-Host "FAILED to load DLL. Error $err : $($sb.ToString())"
} else {
    Write-Host "DLL loaded successfully. Handle: $handle"
}
