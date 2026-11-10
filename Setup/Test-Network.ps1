[CmdletBinding()]
param([ValidateRange(1024,65000)][int]$Port = 17777, [switch]$Pilot,
    [ValidateSet(20,100)][int]$Units = 20, [switch]$Benchmark, [switch]$Rendered, [switch]$PilotBenchmark,
    [ValidateRange(0,200)][int]$LagMS = 0, [ValidateRange(0,10)][int]$LossPercent = 0,
    [ValidateRange(5,60)][int]$Seconds = 20, [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
if ($Pilot -and $Benchmark) { throw 'Choose Pilot or Benchmark, not both' }
if ($PilotBenchmark -and !$Benchmark) { throw 'PilotBenchmark requires Benchmark' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$serverLog = Join-Path $PSScriptRoot "Battle-Net-$stamp-server.log"
$common = @('-unattended', '-nosound', '-nosplash', '-nullrhi')
$common += "-BattleNetUnits=$Units"
$common += @("-PktLag=$LagMS", "-PktLoss=$LossPercent")
$reports = @()
if ($Benchmark) {
    $reportFolder = Join-Path $PSScriptRoot 'NetworkRuns'
    [IO.Directory]::CreateDirectory($reportFolder) | Out-Null
    $common += @('-BattleNetBenchmark', "-BattleNetSeconds=$Seconds")
}
$server = $null; $clients = @(); $clientLogs = @()
try {
    $arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype?game=/Script/FutureBattle.BattleNetGameMode', '-server', "-port=$Port", ('-abslog="' + $serverLog + '"')) + $common
    $server = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $readyUntil = (Get-Date).AddSeconds(45)
    do {
        if ($server.HasExited) { throw "Network server exited: $serverLog" }
        if ((Test-Path -LiteralPath $serverLog) -and (Get-Content -LiteralPath $serverLog -Raw) -match "BATTLE_READY Units=$Units") { break }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $readyUntil)
    if ((Get-Date) -ge $readyUntil) { throw "Network server did not become ready: $serverLog" }
    for ($index = 0; $index -lt 2; $index++) {
        $log = Join-Path $PSScriptRoot "Battle-Net-$stamp-client$index.log"; $clientLogs += $log
        $probeFlag = if ($Benchmark) { '-BattleNetBenchmark' } elseif ($Pilot) { '-BattleNetPilotProbe' } else { '-BattleNetProbe' }
        $arguments = @(('"' + $project + '"'), "127.0.0.1:$Port", '-game', $probeFlag, ('-abslog="' + $log + '"')) + $common
        if ($Benchmark) {
            $report = Join-Path $reportFolder "$stamp-$Units-client$index.json"; $reports += $report
            $arguments += ('-BattleNetReport="' + $report + '"')
            if ($PilotBenchmark) { $arguments += '-BattleNetBenchmarkPilot' }
            if ($Rendered) { $arguments += ('-BattleNetScreenshot="' + [IO.Path]::ChangeExtension($report, '.png') + '"') }
        }
        if ($Rendered) { $arguments = @($arguments | Where-Object { $_ -ne '-nullrhi' }) + @('-RenderOffscreen', '-ResX=1280', '-ResY=720', '-NoVSync') }
        $clients += Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    }
    foreach ($client in $clients) { if (!$client.WaitForExit(90000)) { throw 'Network probe timed out' } }
    $teams = @()
    foreach ($log in $clientLogs) {
        $content = Get-Content -LiteralPath $log -Raw
        $content -split "`n" | Where-Object { $_ -match 'BATTLE_NET_(PRIVATE|RESULT)' } | Write-Output
        if ($content -notmatch 'BATTLE_NET_RESULT success=1 team=([01])' -or $content -match 'Fatal error:|Assertion failed:') { throw "Network probe failed: $log" }
        $teams += $Matches[1]
    }
    if (($teams | Sort-Object -Unique).Count -ne 2) { throw 'Connections did not receive distinct teams' }
    foreach ($report in $reports) {
        $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        if (!$result.success) { throw "Network benchmark failed: $report" }
        [pscustomobject]@{Team=$result.team; Units=$result.unit_count; ReceiveBps=[Math]::Round($result.received_bytes_per_second); SendBps=[Math]::Round($result.sent_bytes_per_second); RTTms=[Math]::Round($result.rpc_round_trip_ms.mean,2); FrameMS=[Math]::Round($result.client_frame_wall_ms.mean,2); Visible=$result.peak_visible_units; Report=$report} | Format-List
    }
    if ($Pilot) {
        $disconnectUntil = (Get-Date).AddSeconds(5)
        do {
            $serverContent = Get-Content -LiteralPath $serverLog -Raw
            if (($serverContent -split "`n" | Where-Object { $_ -match 'BATTLE_NET_CONTROL.*Released=1 AI=1' }).Count -ge 6) { break }
            Start-Sleep -Milliseconds 200
        } while ((Get-Date) -lt $disconnectUntil)
        if (($serverContent -split "`n" | Where-Object { $_ -match 'BATTLE_NET_CONTROL.*Released=1 AI=1' }).Count -lt 6) { throw 'Disconnect did not return both controlled units to AI' }
    }
    Write-Output "Network test passed: $serverLog"
} finally {
    foreach ($process in @($clients) + @($server)) { if ($null -ne $process -and !$process.HasExited) { Stop-Process -Id $process.Id } }
}
