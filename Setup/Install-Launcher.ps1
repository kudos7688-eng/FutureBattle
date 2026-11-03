$ErrorActionPreference = 'Stop'
$path = Join-Path $PSScriptRoot 'EpicInstaller-20.3.3.msi'
$expected = 'D55D79710EDFEAA107A62F2EA007BEC1EEEAC8B715A43C2BA3858EEC7BC60337'
if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected) { throw 'Launcher installer hash mismatch' }
$sig = Get-AuthenticodeSignature -LiteralPath $path
if ($sig.Status -ne 'Valid') { throw 'Launcher installer signature invalid' }
Write-Output "Verified Launcher: $($sig.SignerCertificate.Subject)"
$log = Join-Path $PSScriptRoot 'Launcher-install.log'
for ($attempt = 0; $attempt -lt 60; $attempt++) {
    $p = Start-Process -FilePath 'msiexec.exe' -ArgumentList @('/i',('"'+$path+'"'),'/qn','/norestart','/L*v',('"'+$log+'"')) -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    if ($p.ExitCode -ne 1618) { break }
    Write-Output 'Windows Installer is busy; waiting 20 seconds.'
    Start-Sleep -Seconds 20
}
Write-Output "Launcher installer exit code: $($p.ExitCode)"
if ($p.ExitCode -notin @(0,3010)) { throw 'Launcher installation failed; inspect Launcher-install.log' }
