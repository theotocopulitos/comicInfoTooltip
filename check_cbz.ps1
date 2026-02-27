Add-Type -AssemblyName System.IO.Compression.FileSystem
$path = 'G:\tg-DOWNLOADER\Comic En Espanol\Avengers vs X-Men 002 (www.ElAbueloSawa.com).cbz'
if (-not (Test-Path $path)) {
    Write-Host "File not found: $path"
    exit
}
$zip = [System.IO.Compression.ZipFile]::OpenRead($path)
Write-Host "Total entries: $($zip.Entries.Count)"
Write-Host "Entries matching xml/comic:"
foreach ($e in $zip.Entries) {
    if ($e.FullName -match 'xml|comic') { Write-Host "  $($e.FullName)" }
}
Write-Host "First 5 entries:"
$zip.Entries | Select-Object -First 5 | ForEach-Object { Write-Host "  $($_.FullName)" }
$zip.Dispose()
