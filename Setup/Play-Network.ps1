[CmdletBinding()]
param([ValidateRange(1024,65000)][int]$Port = 17777, [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$serverExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$serverLog = Join-Path $PSScriptRoot "Battle-Net-Play-$stamp-server.log"
$server = $null; $clients = @()
try {
    $arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype?game=/Script/FutureBattle.BattleNetGameMode', '-server', '-nullrhi', '-unattended', '-nosound', "-port=$Port", ('-abslog="' + $serverLog + '"'))
    $server = Start-Process -FilePath $serverExe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $deadline = (Get-Date).AddSeconds(45)
    do {
        if ($server.HasExited) { throw "Server exited: $serverLog" }
        if ((Test-Path -LiteralPath $serverLog) -and (Get-Content -LiteralPath $serverLog -Raw) -match 'BATTLE_READY Units=20') { break }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)
    if ((Get-Date) -ge $deadline) { throw "Server startup timed out: $serverLog" }
    for ($index = 0; $index -lt 2; $index++) {
        $log = Join-Path $PSScriptRoot "Battle-Net-Play-$stamp-client$index.log"
        $arguments = @(('"' + $project + '"'), "127.0.0.1:$Port", '-game', '-windowed', '-ResX=960', '-ResY=600', "-WinX=$($index * 500)", "-WinY=$($index * 100)", '-nosound', ('-abslog="' + $log + '"'))
        $clients += Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    }
    Write-Output 'Two tactical clients started. Close both game windows to stop this local server.'
    foreach ($client in $clients) { $client.WaitForExit() }
} finally {
    # Only the dedicated process created by this invocation is stopped. Never terminate another editor.
    if ($null -ne $server -and !$server.HasExited) { Stop-Process -Id $server.Id }
}
