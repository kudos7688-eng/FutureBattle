[CmdletBinding()]
param([string]$OutputName='stage1',[ValidateSet(1,2)][int]$KnownTargetFire=1,[ValidateSet(0,1)][int]$KnownMoveMarch=1,[ValidateSet(0,1)][int]$KnownMoveDefendMarch=1,[ValidateRange(0,60)][double]$KnownMoveCommit=12,[ValidateRange(0.1,5)][double]$KnownMoveWait=1.5,[ValidateRange(0,25000)][int]$KnownMoveRange=2500,[ValidateRange(0,50000)][int]$KnownMoveSquadRadius=5000,[ValidateRange(0,500)][int]$KnownMoveMax=0,[ValidateSet(0,3)][int]$SpotTrack=0,[switch]$Rendered)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Use a simple output name'}
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/KnownTargetFire-20261126/$OutputName"
if(Test-Path $out){throw "Already exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_C','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleKnownTargetFireTrial',"-BattleKnownTargetFire=$KnownTargetFire",("-BattleKnownMoveCommit="+$KnownMoveCommit.ToString([Globalization.CultureInfo]::InvariantCulture)),("-BattleKnownMoveWait="+$KnownMoveWait.ToString([Globalization.CultureInfo]::InvariantCulture)),"-BattleKnownMoveRange=$KnownMoveRange","-BattleKnownMoveMarch=$KnownMoveMarch","-BattleKnownMoveDefendMarch=$KnownMoveDefendMarch","-BattleKnownMoveSquadRadius=$KnownMoveSquadRadius","-BattleKnownMoveMax=$KnownMoveMax","-BattleSpotTrack=$SpotTrack",'-BattleStances=1','-BattleCoverActions=1','-BattleCoverFire=1','-BattleCoverPeekSight=1','-BattleCoverValidation=1','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0',('-BattleSightExperiment="'+$out+'"'),('-abslog="'+$out+'/run.log"'))
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720')}else{$arguments+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$arguments;trial='BattleKnownTargetFireTrial';knownTargetFire=$KnownTargetFire;knownMoveCommit=$KnownMoveCommit;knownMoveWait=$KnownMoveWait;knownMoveRange=$KnownMoveRange;knownMoveMarch=$KnownMoveMarch;knownMoveDefendMarch=$KnownMoveDefendMarch;knownMoveSquadRadius=$KnownMoveSquadRadius;knownMoveMax=$KnownMoveMax;spotTrack=$SpotTrack;rendered=[bool]$Rendered}|ConvertTo-Json -Depth 8|Set-Content "$out/manifest.json" -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$timedOut=!$p.WaitForExit(180000)
if($timedOut){Stop-Process -Id $p.Id;$p.WaitForExit()}
$p.Refresh()
$log=if(Test-Path "$out/run.log"){Get-Content "$out/run.log" -Raw}else{''}
$expected=@('fixture','other_squad_open','other_squad_peek','blocked_first_second_fires','stale_no_revival','track_aim','track_still_needs_sensor','budget_deferred','budget_fifo','move_order','support_target','weapon_gate','unreported_scout','aim_wall_separate','blocked_first_peek_second','delivered_original_expiry','sensor_fov','sensor_range','search_interval','selected_rechecked','all_blocked_no_move','legacy_peek_priority','old_aim_wall_ignored','confirmed_aim_held','known_damage_counter','ordinary_not_known','confirmed_stance_aim')
if($KnownTargetFire -eq 2){$expected+=@('move_edge_specified_goal','move_edge_specified_peek_ray','move_edge_disabled_no_move_fire','move_edge_old_places_blocked','move_edge_arrive_personal_fire','move_edge_budget','move_arrive_personal_fire','move_first_shot_once','move_order_change','move_priority_gates','move_distance_mission_limits','move_shared_limit','move_no_double_start','move_ray_budget','move_shared_ray_budget','move_geometry_budget','move_budget_fifo','move_cooldown','move_budget_atomic','move_interval_diagnostic','move_no_partial_geometry','move_fixture_left_hidden_fire','move_fixture_right_hidden_fire');if($KnownMoveMarch -eq 0){$expected+=@('move_march_gate','move_march_follow_gate')}else{$expected+=@('move_march_arrive_fire','move_march_resume','move_march_near_gate','move_march_sector','move_march_squad_radius')}}
if($KnownTargetFire -eq 2){if($KnownMoveCommit -eq 0){$expected+='move_stale_return';if($KnownMoveMarch -eq 1){$expected+='move_march_stale_resume'}}else{$expected+=@('move_commit_arrive_fire','move_commit_unseen_return','move_commit_timeout','move_commit_refresh','move_commit_sensor_wall','move_commit_muzzle_wall','move_commit_current_aim','move_commit_wait_fifo','move_commit_cancel_fifo','move_commit_actor_speed');if($KnownMoveMarch -eq 1){$expected+=@('move_commit_march_arrive_fire','move_commit_march_unseen_return')}}}
if($KnownTargetFire -eq 2){if($KnownMoveDefendMarch -eq 0){$expected+=@('move_defend_march_disabled','move_defend_march_disabled_walk')}else{$expected+=@('move_defend_march_arrive_fire','move_defend_march_resume','move_defend_march_sector','move_defend_march_reverse_rejected','move_defend_march_stale_resume','move_defend_march_combat_stop')}}
$checks=@([regex]::Matches($log,'BATTLE_KNOWN_FIRE_CHECK name=(\w+) ok=([01])')|ForEach-Object{@{name=$_.Groups[1].Value;passed=$_.Groups[2].Value -eq '1'}})
$diagnostics=@{}
foreach($entry in [regex]::Matches($log,'BATTLE_KNOWN_FIRE_DIAG (known_(?:fire|move)_\w+)=(\d+(?:\.\d+)?)')){$diagnostics[$entry.Groups[1].Value]=[double]::Parse($entry.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)}
$missing=@($expected|Where-Object {$label=$_;@($checks|Where-Object {$_.name -eq $label}).Count -ne 1})
$ok=!$timedOut -and $p.ExitCode -eq 0 -and $log -match 'BATTLE_KNOWN_FIRE_RESULT failures=0(?:\r?\n|$)' -and $checks.Count -eq $expected.Count -and $missing.Count -eq 0 -and @($checks|Where-Object {!$_.passed}).Count -eq 0 -and $log -notmatch 'Fatal error:|Assertion failed:'
# Keep the actual rendering adapter evidence beside the machine inventory; NullRHI has no rendering GPU.
$renderAdapter=@($log -split "`n"|Where-Object {$_ -match 'LogD3D12RHI:.*(Adapter|Description|DedicatedVideoMemory)|LogRHI:.*(RHI|GPU)'})
@{passed=$ok;timedOut=$timedOut;exitCode=$p.ExitCode;checks=$checks;missing=$missing;cover_diagnostics=$diagnostics;renderAdapterLog=$renderAdapter;nullRHI=(!$Rendered)}|ConvertTo-Json -Depth 8|Set-Content "$out/result.json" -Encoding utf8
$log -split "`n"|Where-Object {$_ -match 'BATTLE_KNOWN_FIRE_(CHECK|RESULT|DIAG)'}
if(!$ok){throw "Known target fire trial failed: $out"}
Write-Output "Evidence: $out"
