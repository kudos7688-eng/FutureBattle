[CmdletBinding()]
param([string]$OutputName='pilot',[ValidateSet(0,1)][int]$Stable=1,[switch]$Rendered,[ValidateRange(-1,3)][int]$Scene=-1)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple output name'}
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/MovementStability-20261111/$OutputName"
if(Test-Path $out){throw "Already exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;stable=$Stable;scene=$Scene;rendered=[bool]$Rendered}|ConvertTo-Json -Depth 8|Set-Content "$out/manifest.json" -Encoding utf8
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleMovementTrial',"-BattleStableMovement=$Stable",('-abslog="'+$out+'/run.log"'))
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+$out+'/screen"'))}else{$arguments+='-nullrhi'}
$arguments+="-BattleMovementScene=$Scene"
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(200000)){Stop-Process -Id $p.Id;throw 'Movement trial timed out'}
$log=Get-Content "$out/run.log" -Raw
$ok=$p.ExitCode -eq 0 -and $log -match 'BATTLE_MOVE_RESULT failures=0' -and $log -notmatch 'Fatal error:|Assertion failed:'
$metrics=@([regex]::Matches($log,'BATTLE_MOVE_METRIC[^\r\n]+')|ForEach-Object {$_.Value})
$frames=@([regex]::Matches($log,'BATTLE_MOVE_FRAME[^\r\n]+')|ForEach-Object {$_.Value})
$expected=if($Scene -lt 0){4}else{1}
if($Stable -eq 1 -and ([regex]::Matches($log,'BATTLE_MOVE_CHECK PASS')).Count -ne $expected){$ok=$false}
@{passed=$ok;dll=(Get-FileHash $dll).Hash;metrics=$metrics;frames=$frames}|ConvertTo-Json -Depth 5|Set-Content "$out/result.json" -Encoding utf8
$metrics|Write-Output
if(!$ok -or $metrics.Count -ne $expected){throw "Trial failed: $out"}
if($Rendered){$names=@('dense','narrow','counterflow','gather');if($Scene -ge 0){$names=@($names[$Scene])};foreach($name in $names){if(!(Test-Path "$out/screen-$name.png")){throw "Missing screenshot: $name"}}}
Write-Output "Evidence: $out"
