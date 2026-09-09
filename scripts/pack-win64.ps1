param([string]$Root)

if (-not $Root) { $Root = "$PSScriptRoot\.." }
$Root     = (Resolve-Path $Root).Path
$Exe      = Join-Path $Root "bin\3EChan.exe"
$ResDir   = Join-Path $Root "res"
$OutDir   = Join-Path $Root "shipping"
$OutZip   = Join-Path $OutDir "3EChan-win64.zip"
$TmpDir   = Join-Path $env:TEMP "3EChan-win64-pack"
$Stage    = Join-Path $TmpDir "3EChan-win64"

if (Test-Path $TmpDir) { Remove-Item $TmpDir -Recurse -Force }
New-Item -ItemType Directory -Force $Stage | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null

$hash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLower()
"$hash  3EChan.exe" | Set-Content -Encoding UTF8 (Join-Path $Stage "SHA256SUM.txt")

Copy-Item $Exe (Join-Path $Stage "3EChan.exe")

# v1.1.1 migration compatibility:
# v1.1.0 updater still relaunches rechan.exe after extracting the update.
# Remove this compatibility copy from a later release after migration.
Copy-Item $Exe (Join-Path $Stage "rechan.exe")
Copy-Item (Join-Path $ResDir "pc") (Join-Path $Stage "pc") -Recurse

if (Test-Path $OutZip) { Remove-Item $OutZip }
Compress-Archive -Path "$Stage\*" -DestinationPath $OutZip

Remove-Item $TmpDir -Recurse -Force
Write-Host "Packed: $OutZip"
