[CmdletBinding()]
param([string]$OutputName='pilot',[switch]$Rendered,[switch]$Probe,[ValidateSet(0,1)][int]$Enabled=1)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple output name'}
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/ContactMovement-20261111/$OutputName"
if(Test-Path $out){throw "Already exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;enabled=$Enabled;probe=[bool]$Probe;rendered=[bool]$Rendered}|ConvertTo-Json -Depth 8|Set-Content "$out/manifest.json" -Encoding utf8
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleContactTrial','-BattleStableMovement=1',"-BattleContactAvoidance=$Enabled",('-abslog="'+$out+'/run.log"'))
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+$out+'/screen"'))}else{$arguments+='-nullrhi'}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id;throw 'Contact trial timed out'}
$log=Get-Content "$out/run.log" -Raw
$checks=@([regex]::Matches($log,'BATTLE_CONTACT_CHECK[^\r\n]+')|ForEach-Object {$_.Value})
$ok=$p.ExitCode -eq 0 -and $log -match 'BATTLE_CONTACT_RESULT failures=0' -and $checks.Count -eq 3
@{passed=$ok;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash;checks=$checks}|ConvertTo-Json -Depth 5|Set-Content "$out/result.json" -Encoding utf8
$checks|Write-Output
if($log -match 'Fatal error:|Assertion failed:' -or $checks.Count -ne 3 -or (!$ok -and !$Probe)){throw "Contact trial failed: $out"}
Write-Output "Evidence: $out"

