# Test whether our shell extension COM object is working
$clsid = "{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}"

# Find a test CBZ or CBR file
$testFile = Get-ChildItem "$env:USERPROFILE" -Recurse -Include "*.cbz","*.cbr" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $testFile) {
    $testFile = Get-ChildItem "C:\" -Recurse -Include "*.cbz","*.cbr" -ErrorAction SilentlyContinue -Depth 4 | Select-Object -First 1
}

Write-Host "CLSID registered: $clsid"
if ($testFile) { Write-Host "Test file: $($testFile.FullName)" } else { Write-Host "Test file: NONE FOUND" }

# Check all registration keys
Write-Host "`n--- Registry Check ---"
$keys = @(
    "HKLM:\SOFTWARE\Classes\CLSID\$clsid\InprocServer32",
    "HKLM:\SOFTWARE\Classes\.cbz\shellex\{00021500-0000-0000-C000-000000000046}",
    "HKLM:\SOFTWARE\Classes\.cbr\shellex\{00021500-0000-0000-C000-000000000046}",
    "HKLM:\SOFTWARE\Classes\cYo.ComicRack\shellex\{00021500-0000-0000-C000-000000000046}",
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"
)

foreach ($k in $keys) {
    if (Test-Path $k) {
        $val = (Get-ItemProperty $k -ErrorAction SilentlyContinue).'(default)'
        Write-Host "  OK: $k = $val"
    } else {
        # Try HKCR
        $hkcr = $k -replace "HKLM:\\SOFTWARE\\Classes", "HKCR:"
        if (Test-Path $hkcr) {
            $val = (Get-ItemProperty $hkcr -ErrorAction SilentlyContinue).'(default)'
            Write-Host "  OK (HKCR): $hkcr = $val"
        } else {
            Write-Host "  MISSING: $k"
        }
    }
}

# Check approved
$approved = (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" -ErrorAction SilentlyContinue)."$clsid"
if ($approved) { Write-Host "  Approved: $approved" } else { Write-Host "  Approved: NOT FOUND" }

# Try to instantiate the COM object via PowerShell
Write-Host "`n--- COM Instantiation Test ---"
try {
    $type = [Type]::GetTypeFromCLSID([Guid]$clsid.Trim('{}'))
    $obj = [Activator]::CreateInstance($type)
    Write-Host "  COM object created: $obj"
} catch {
    Write-Host "  FAILED to create COM object: $($_.Exception.Message)"
}
