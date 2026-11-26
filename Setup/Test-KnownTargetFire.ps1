[CmdletBinding()]
param([string]$OutputName='stage1',[ValidateSet(0,3)][int]$SpotTrack=0,[switch]$Rendered)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple output name'}
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/KnownTargetFire-20261126/$OutputName"
if(Test-Path $out){throw "Already exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleKnownTargetFireTrial','-BattleKnownTargetFire=1',"-BattleSpotTrack=$SpotTrack",'-BattleStances=1','-BattleCoverActions=1','-BattleCoverFire=1','-BattleCoverPeekSight=1','-BattleCoverValidation=1','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0',('-BattleSightExperiment="'+$out+'"'),('-abslog="'+$out+'/run.log"'))
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$arguments;trial='BattleKnownTargetFireTrial';knownTargetFire=1;spotTrack=$SpotTrack;rendered=[bool]$Rendered}|ConvertTo-Json -Depth 8|Set-Content "$out/manifest.json" -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$timedOut=!$p.WaitForExit(180000)
if($timedOut){Stop-Process -Id $p.Id;$p.WaitForExit()}
$p.Refresh()
$log=if(Test-Path "$out/run.log"){Get-Content "$out/run.log" -Raw}else{''}
$expected=@('fixture','other_squad_open','other_squad_peek','blocked_first_second_fires','stale_no_revival','track_aim','track_still_needs_sensor','budget_deferred','budget_fifo','move_order','support_target','weapon_gate','unreported_scout','aim_wall_separate','blocked_first_peek_second','delivered_original_expiry','sensor_fov','sensor_range','search_interval','selected_rechecked','all_blocked_no_move','legacy_peek_priority')
$checks=@([regex]::Matches($log,'BATTLE_KNOWN_FIRE_CHECK name=(\w+) ok=([01])')|ForEach-Object{@{name=$_.Groups[1].Value;passed=$_.Groups[2].Value -eq '1'}})
$diagnostics=@{}
foreach($entry in [regex]::Matches($log,'BATTLE_KNOWN_FIRE_DIAG (known_fire_\w+)=(\d+)')){$diagnostics[$entry.Groups[1].Value]=[long]$entry.Groups[2].Value}
$missing=@($expected|Where-Object {$label=$_;@($checks|Where-Object {$_.name -eq $label}).Count -ne 1})
$ok=!$timedOut -and $p.ExitCode -eq 0 -and $log -match 'BATTLE_KNOWN_FIRE_RESULT failures=0(?:\r?\n|$)' -and $checks.Count -eq $expected.Count -and $missing.Count -eq 0 -and @($checks|Where-Object {!$_.passed}).Count -eq 0 -and $log -notmatch 'Fatal error:|Assertion failed:'
# Keep the actual rendering adapter evidence beside the machine inventory; NullRHI has no rendering GPU.
$renderAdapter=@($log -split "`n"|Where-Object {$_ -match 'LogD3D12RHI:.*(Adapter|Description|DedicatedVideoMemory)|LogRHI:.*(RHI|GPU)'})
@{passed=$ok;timedOut=$timedOut;exitCode=$p.ExitCode;checks=$checks;missing=$missing;cover_diagnostics=$diagnostics;renderAdapterLog=$renderAdapter;nullRHI=(!$Rendered)}|ConvertTo-Json -Depth 8|Set-Content "$out/result.json" -Encoding utf8
$log -split "`n"|Where-Object {$_ -match 'BATTLE_KNOWN_FIRE_(CHECK|RESULT|DIAG)'}
if(!$ok){throw "Known target fire trial failed: $out"}
Write-Output "Evidence: $out"
