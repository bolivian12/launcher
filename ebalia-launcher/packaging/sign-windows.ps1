param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$CertificateThumbprint,
    [string]$TimestampUrl = 'https://timestamp.digicert.com'
)
$ErrorActionPreference = 'Stop'
# Use a trusted Authenticode certificate already available in the Windows certificate store.
# Never embed a private key or password in this repository.
if (!(Test-Path -LiteralPath $Executable)) { throw 'Executable not found' }
& signtool sign /sha1 $CertificateThumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 /d 'EBALIA Launcher' $Executable
if ($LASTEXITCODE -ne 0) { throw 'Authenticode signing failed' }
& signtool verify /pa /all /v $Executable
if ($LASTEXITCODE -ne 0) { throw 'Authenticode verification failed' }
Get-FileHash -Algorithm SHA256 -LiteralPath $Executable
