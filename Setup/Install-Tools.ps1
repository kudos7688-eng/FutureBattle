$ErrorActionPreference = 'Stop'
function Get-VerifiedInstaller($Name, $Url, $Hash) {
    $path = Join-Path $PSScriptRoot $Name
    if (!(Test-Path -LiteralPath $path)) {
        & curl.exe --silent --show-error --fail --location --connect-timeout 20 --max-time 900 --output $path $Url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $Name" }
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $Hash) { throw "Hash mismatch: $Name" }
    $sig = Get-AuthenticodeSignature -LiteralPath $path
    if ($sig.Status -ne 'Valid') { throw "Invalid signature: $Name" }
    Write-Output "Verified $Name : $($sig.SignerCertificate.Subject)"
    return $path
}

$gitResults = Get-VerifiedInstaller 'Git-2.56.0.4-64-bit.exe' 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.4/Git-2.56.0.4-64-bit.exe' '4961a4a8cc778d8b6e00a5819e5fe4d078b754555aff7aedc2d77a5d74c2de80'
$gitResults | Write-Output
$p = Start-Process -FilePath $gitResults[-1] -ArgumentList '/VERYSILENT /NORESTART /SP-' -WindowStyle Hidden -Wait -PassThru
Write-Output "Git installer exit code: $($p.ExitCode)"
if ($p.ExitCode -notin @(0,3010)) { throw 'Git installation failed' }

$vsResults = Get-VerifiedInstaller 'vs_Community.exe' 'https://download.visualstudio.microsoft.com/download/pr/6bd63d7f-1818-4e03-a552-9feb11496c19/fec1b3df4cd2dd488083008dea194201f7f14574a7ce8f4eba943c9b6f4a01f3/vs_Community.exe' 'fec1b3df4cd2dd488083008dea194201f7f14574a7ce8f4eba943c9b6f4a01f3'
$vsResults | Write-Output
$p = Start-Process -FilePath $vsResults[-1] -ArgumentList '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.NativeGame --includeRecommended' -WindowStyle Hidden -Wait -PassThru
Write-Output "Visual Studio installer exit code: $($p.ExitCode)"
if ($p.ExitCode -notin @(0,3010)) { throw 'Visual Studio installation failed' }
