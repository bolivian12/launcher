param(
    [Parameter(Mandatory=$true)][string]$PackagePath,
    [switch]$RequireSignature
)
$ErrorActionPreference = 'Stop'
$folder = (Resolve-Path -LiteralPath $PackagePath).Path
$exe = Join-Path $folder 'ebalia-launcher.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Choose the folder containing ebalia-launcher.exe and its DLLs.' }
$signature = Get-AuthenticodeSignature -LiteralPath $exe
Write-Output "Authenticode: $($signature.Status)"
if ($RequireSignature -and $signature.Status -ne 'Valid') { throw 'A valid publisher signature is required for this release.' }
$status = Get-MpComputerStatus
if (!$status.AntivirusEnabled -or !$status.AMServiceEnabled) { throw 'Microsoft Defender is unavailable. This package has NOT been scanned.' }
$scanner = Get-ChildItem "$env:ProgramData\Microsoft\Windows Defender\Platform\*\MpCmdRun.exe" |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$scanner) { throw 'Microsoft Defender scanner was not found.' }
# Scan without changing Defender preferences, adding exclusions, or removing packaged files.
& $scanner.FullName -Scan -ScanType 3 -File $folder -DisableRemediation
if ($LASTEXITCODE -ne 0) { throw 'Defender reported a detection or scan failure. Review the scanner output; do not publish this build as clean.' }
Write-Output "Defender scan completed with engine $($status.AMEngineVersion), signatures $($status.AntivirusSignatureVersion)."
Get-ChildItem -LiteralPath $folder -File -Recurse | Get-FileHash -Algorithm SHA256
Write-Output 'A clean scan applies to these exact files and signatures; it does not guarantee future results or SmartScreen reputation.'
