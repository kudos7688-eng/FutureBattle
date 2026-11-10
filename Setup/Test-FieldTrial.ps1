[CmdletBinding()]
param([int[]]$Seeds = @(7,19,43), [ValidateSet("Actors","Individual","Squad")][string[]]$Modes=@("Actors","Individual","Squad"), [switch]$WeaponLoadouts, [switch]$Diagnostics, [switch]$LargeField, [switch]$LegacyVisuals, [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project=(Resolve-Path "$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject").Path
$stamp=Get-Date -Format yyyyMMdd-HHmmss
$folder=Join-Path $PSScriptRoot 'Benchmarks'
[IO.Directory]::CreateDirectory($folder) | Out-Null
if ($Diagnostics) { $stamp += "-diag" }
if ($WeaponLoadouts) { $stamp += "-armed" }
if ($LargeField) { $stamp += "-500wide" }
if ($LegacyVisuals) { $stamp += "-legacy" }
for ($round=0; $round -lt $Seeds.Count; $round++) {
    $seed=$Seeds[$round]
    for ($offset=0; $offset -lt $Modes.Count; $offset++) {
        $mode=$modes[($round+$offset)%$Modes.Count]
        $name="$stamp-field-$seed-$mode"
        $capture=Join-Path $folder "$name.png"
        $log=Join-Path $PSScriptRoot "Battle-$name.log"
        $map=if($LargeField){'/Game/Prototype/Lvl_BattleField500'}else{'/Game/Prototype/Lvl_BattlePrototype'}
        $count=if($LargeField){500}else{1000}
        $argsList=@(('"' + $project + '"'), $map,'-game','-unattended','-nosound','-nosplash',
            '-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync',"-BattleUnits=$count","-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",
            ('-BattleMovingCapture="' + $capture + '"'),('-abslog="' + $log + '"'))
        if ($Diagnostics) { $argsList += "-BattleFieldDiagnostics" }
        if ($WeaponLoadouts) { $argsList += "-BattleWeaponLoadouts" }
        if ($LegacyVisuals) { $argsList += "-BattleLegacyWeaponVisuals" }
        $p=Start-Process $editor -ArgumentList $argsList -WindowStyle Hidden -PassThru
        if (!$p.WaitForExit(600000)) { $p.Kill(); throw "Timeout: $log" }
        $line=(Select-String -Path $log -Pattern 'FIELD_TRIAL mode=' | Select-Object -Last 1).Line
        if ($p.ExitCode -ne 0 -or $line -notmatch 'success=1' -or !(Test-Path $capture)) { throw "Field trial failed: $line ($log)" }
        if ($Diagnostics -and !(Test-Path (Join-Path $folder "$name-diagnostics.json"))) { throw "Diagnostics missing: $name" }
        $line
        Add-Content (Join-Path $folder "$stamp-field-results.txt") $line
    }
}
