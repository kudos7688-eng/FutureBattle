[CmdletBinding()]
param([ValidateRange(1024,65000)][int]$Port = 17779, [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$serverLog = Join-Path $PSScriptRoot "Battle-Slots-$stamp-server.log"
$common = @('-nullrhi', '-nosound', '-unattended', '-nosplash')
$server = $null; $clients = @(); $logs = @()
function Wait-Log([string]$Path, [string]$Pattern, [int]$Seconds = 45) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        if (Test-Path -LiteralPath $Path) {
            $content = Get-Content -LiteralPath $Path -Raw
            if ($content -match 'Fatal error:|Assertion failed:') { throw "Fatal error in $Path" }
            if ($content -match $Pattern) { return $content }
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)
    throw "Timed out waiting for $Pattern in $Path"
}
function Start-TestClient([int]$Index, [int]$LeaveAfter = 0) {
    $log = Join-Path $PSScriptRoot "Battle-Slots-$stamp-client$Index.log"
    $arguments = @(('"' + $project + '"'), "127.0.0.1:$Port", '-game', '-BattleNetHoldProbe', "-BattleNetLeaveAfter=$LeaveAfter", ('-abslog="' + $log + '"')) + $common
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $script:clients += $process; $script:logs += $log
}
try {
    $arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype?game=/Script/FutureBattle.BattleNetGameMode', '-server', "-port=$Port", ('-abslog="' + $serverLog + '"')) + $common
    $server = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $null = Wait-Log $serverLog 'BATTLE_READY Units=20'
    Start-TestClient 0 30
    Start-TestClient 1
    $first = Wait-Log $logs[0] 'BATTLE_NET_SLOT team=([01]) running=1 own=10 enemy=0'
    $firstTeam = [regex]::Match($first, 'BATTLE_NET_SLOT team=([01]) running=1').Groups[1].Value
    $otherTeam = 1 - [int]$firstTeam
    $null = Wait-Log $logs[1] "BATTLE_NET_SLOT team=$otherTeam running=1 own=10 enemy=0"
    Start-TestClient 2
    $null = Wait-Log $logs[2] 'This prototype has two command slots'
    if (!$clients[2].HasExited) { Stop-Process -Id $clients[2].Id }
    # Client zero performs a normal engine shutdown; the server releases its command slot.
    $null = Wait-Log $logs[1] 'BATTLE_NET_SLOT team=[01] running=1[\s\S]*BATTLE_NET_SLOT team=[01] running=0 own=10 enemy=0' 50
    if (!$clients[0].WaitForExit(10000)) { throw 'Departing client did not complete normal shutdown' }
    Start-TestClient 3
    $null = Wait-Log $logs[3] "BATTLE_NET_SLOT team=$firstTeam running=1 own=10 enemy=0"
    $joins = @(Select-String -Path $serverLog -Pattern 'BATTLE_NET_JOIN Team=')
    if ($joins.Count -ne 3) { throw "Expected three admitted connections, got $($joins.Count)" }
    Write-Output "BATTLE_NET_SLOTS success=1 full_rejected=1 paused=1 reconnect_team=$firstTeam private_view=1"
    Write-Output "Logs: $serverLog"
} finally {
    foreach ($process in @($clients) + @($server)) { if ($null -ne $process -and !$process.HasExited) { Stop-Process -Id $process.Id } }
}
