[CmdletBinding()]
param([string]$OutputName='pilot',[switch]$Rendered,[switch]$Probe)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple output name'}
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/CoverFirePoint-20261125/$OutputName"
if(Test-Path $out){throw "Already exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;trial="BattleCoverFirePointTrial";stances=1;coverFirePoint=1;probe=[bool]$Probe;rendered=[bool]$Rendered}|ConvertTo-Json -Depth 8|Set-Content "$out/manifest.json" -Encoding utf8
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleCoverFirePointTrial','-BattleStances=1','-BattleCoverFirePoint=1',('-abslog="'+$out+'/run.log"'))
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+$out+'/screen"'))}else{$arguments+='-nullrhi'}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$timedOut=!$p.WaitForExit(180000)
if($timedOut){Stop-Process -Id $p.Id;$p.WaitForExit()}
$p.Refresh()
$log=if(Test-Path "$out/run.log"){Get-Content "$out/run.log" -Raw}else{''}
$checks=@([regex]::Matches($log,'BATTLE_COVERFP_CHECK[^\r\n]+')|ForEach-Object {$_.Value})
$ok=!$timedOut -and $p.ExitCode -eq 0 -and $log -match 'BATTLE_COVERFP_RESULT failures=0(?:\r?\n|$)' -and $checks.Count -eq 6 -and @($checks|Where-Object {$_ -notmatch ' ok=1 ' }).Count -eq 0
@{passed=$ok;timedOut=$timedOut;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash;checks=$checks}|ConvertTo-Json -Depth 5|Set-Content "$out/result.json" -Encoding utf8
$checks|Write-Output
if($timedOut -or $log -match 'Fatal error:|Assertion failed:' -or $checks.Count -ne 6 -or (!$ok -and !$Probe)){throw "Cover fire point trial failed: $out"}
Write-Output "Evidence: $out"

