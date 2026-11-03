$ErrorActionPreference = 'Stop'
$config = 'C:\Users\nanek\AppData\Local\EpicGamesLauncher\Saved\Config\Windows\Engine.ini'
$backup = Join-Path $PSScriptRoot 'Launcher-Engine.ini.before-timeout'
if (Test-Path -LiteralPath $config) {
    if (!(Test-Path -LiteralPath $backup)) { Copy-Item -LiteralPath $config -Destination $backup }
    $content = Get-Content -LiteralPath $config -Raw
} else { $content = '' }
if ($content -match '(?m)^\[HTTP\]') { throw 'HTTP section already exists; inspect before modifying.' }
Get-Process -Name EpicGamesLauncher -ErrorAction SilentlyContinue | Stop-Process
New-Item -ItemType Directory -Path (Split-Path -Parent $config) -Force | Out-Null
Add-Content -LiteralPath $config -Encoding utf8 -Value "`r`n[HTTP]`r`nHttpTotalTimeout=600`r`nHttpConnectionTimeout=60`r`nHttpActivityTimeout=120`r`n"
Write-Output 'Launcher HTTP timeout increased: total=600s, connection=60s, inactivity=120s. Restart required.'
