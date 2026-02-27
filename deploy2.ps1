$src = "C:\Users\Arturo\Documents\Windsurf\git\ComicsShellExtension\CascadeProjects\windsurf-project\x64\Release\ComicTooltipExt.dll"
$dst = "C:\Windows\System32\ComicTooltipExt.dll"

Write-Host "Source: $((Get-Item $src).LastWriteTime)  $((Get-Item $src).Length) bytes"
Write-Host "Dest:   $((Get-Item $dst).LastWriteTime)  $((Get-Item $dst).Length) bytes"

Stop-Process -Name explorer   -Force -ErrorAction SilentlyContinue
Stop-Process -Name dllhost    -Force -ErrorAction SilentlyContinue
Stop-Process -Name Everything -Force -ErrorAction SilentlyContinue
Start-Sleep 3

try {
    [System.IO.File]::Copy($src, $dst, $true)
    Write-Host "Copy OK: $((Get-Item $dst).LastWriteTime)"
} catch {
    Write-Host "Copy FAILED: $_"
}

Start-Process explorer.exe
Write-Host "Explorer started. Press any key to close..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
