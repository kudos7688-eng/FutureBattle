[CmdletBinding()]
param([ValidateSet(500,1000)][int]$Units = 1000, [ValidateRange(1,10)][int]$Repeats = 2,
    [switch]$MovingRendered, [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path "$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject").Path
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$stamp = Get-Date -Format yyyyMMdd-HHmmss
$reference = $null
if ($MovingRendered -and $Units -ne 1000) { throw 'MovingRendered requires 1000 soldiers.' }
if ($MovingRendered) {
    $stamp += '-moving'
    $folder = Join-Path $PSScriptRoot 'Benchmarks'
    [IO.Directory]::CreateDirectory($folder) | Out-Null
}
for ($run = 1; $run -le $Repeats; $run++) {
    $modes = @('Actors','Individual','Squad')
    if ($run % 2 -eq 0) { [array]::Reverse($modes) }
    foreach ($mode in $modes) {
        $log = Join-Path $PSScriptRoot "Battle-SquadTrial-$stamp-$Units-$run-$mode.log"
        $arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game',
            '-nosound','-unattended','-nosplash', "-BattleSquadTrial=$mode", "-BattleUnits=$Units", ('-abslog="' + $log + '"'))
        if ($MovingRendered) {
            $capture = Join-Path $folder "$stamp-$run-$mode.png"
            $arguments += @('-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync',('-BattleMovingCapture="' + $capture + '"'))
        } else { $arguments += '-nullrhi' }
        $process = Start-Process $editor -ArgumentList $arguments -PassThru -WindowStyle Hidden
        if (!$process.WaitForExit(180000)) { $process.Kill(); throw "Timeout: $log" }
        if ($MovingRendered) {
            $line = (Select-String -LiteralPath $log -Pattern 'MOVING_TRIAL mode=' | Select-Object -Last 1).Line
            if ($process.ExitCode -ne 0 -or $line -notmatch 'success=1' -or !(Test-Path -LiteralPath $capture)) { throw "Moving trial failed: $line ($log)" }
            $line
            Add-Content -LiteralPath (Join-Path $folder "$stamp-results.txt") -Value $line
            continue
        }
        $line = (Select-String -LiteralPath $log -Pattern 'SQUAD_TRIAL mode=' | Select-Object -Last 1).Line
        if ($process.ExitCode -ne 0 -or $line -notmatch 'success=1') { throw "Trial failed: $line ($log)" }
        $line
        if ($line -notmatch 'shots=(\d+) hits=(\d+)') { throw "Missing combat counts: $log" }
        $combat = $Matches[0]
        if ($reference -and $reference -ne $combat) { throw "Combat diverged: $reference / $combat ($log)" }
        $reference = $combat
    }
}
