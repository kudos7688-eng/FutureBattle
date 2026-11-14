[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bench=Join-Path $PSScriptRoot 'Benchmarks'
function Read-Json($path){Get-Content -LiteralPath $path -Raw|ConvertFrom-Json}
$snapshots=@{}
foreach($version in @('1','2','3','4','4-r2')){
 $s=Read-Json (Join-Path $bench "BuildSnapshots/flank-stages-$version/manifest.json")
 if((Get-FileHash $s.sourceArchive).Hash -ne $s.sourceArchiveSha256){throw "Changed source archive $version"}
 $snapshots[$version]=$s
}
$cases=@()
foreach($d in Get-ChildItem (Join-Path $bench 'FlankStages-20261112') -Directory -Filter 'stage1-*'){$cases+=@{folder=$d.FullName;build='1'}}
if($cases.Count -ne 12){throw 'Expected twelve local trials'}
foreach($c in @(
 @('FlankStages-20261112/stage2-cover-r5','2'),
 @('SquadFlank-ST3b-20261112/stages3-r1','3'),
 @('SightTrial-20261111/stages4-D-movement','4'),
 @('SightTrial-20261111/stages4-D-geometry-r3','4-r2'),
 @('FlankStages-20261112/stages4-D-cover-r2','4-r2'),
 @('Area-20261112/stages4-final-r2','4-r2'),
 @('MixedSave-ST4b-20261112/stages4-final-r2','4-r2'),
 @('SightTrial-20261111/stages4-D-s7-r2','4-r2'),
 @('SightTrial-20261111/stages4-D-s7-swap-r2','4-r2'),
 @('SightTrial-20261111/stages4-A-s7-r2','4-r2')
)){$cases+=@{folder=(Join-Path $bench $c[0]);build=$c[1]}}
$verified=@()
foreach($c in $cases){
 $m=Read-Json (Join-Path $c.folder 'manifest.json');$r=Read-Json (Join-Path $c.folder 'result.json')
 if(!$r.passed -or $r.exitCode -ne 0){throw "Failed case $($c.folder)"}
 if($m.dll -ne $snapshots[$c.build].dll){throw "DLL mismatch $($c.folder)"}
 if(!$m.machine.gpus -or !$m.machine.cpus -or !$m.machine.memory_bytes -or !$m.machine.os){throw "Missing machine record $($c.folder)"}
 $verified+=@{folder=$c.folder;build=$c.build;checks=$r.checks;passed=$true}
}
$latest=$snapshots['4-r2']
foreach($f in $latest.files){if((Get-FileHash $f.path).Hash -ne $f.sha256){throw "Current source/config changed: $($f.path)"}}
if((Get-FileHash (Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')).Hash -ne $latest.dll){throw 'Current DLL changed'}
foreach($f in $latest.maps){if((Get-FileHash $f.path).Hash -ne $f.sha256){throw "Current map changed: $($f.path)"}}
foreach($f in $snapshots['1'].maps){if((Get-FileHash $f.path).Hash -ne $f.sha256){throw "Original map changed: $($f.path)"}}
foreach($step in @('1','2','3','4-r2')){
 $machine=Read-Json (Join-Path $PSScriptRoot "FlankStages-basic$step-machine.json")
 if(!$machine.machine.gpus -or !$machine.machine.cpus -or !$machine.machine.memory_bytes -or !$machine.machine.os){throw "Missing basic machine record $step"}
 $log=Get-Content (Join-Path $PSScriptRoot "FlankStages-basic$step.txt") -Raw
 if($log -notmatch 'BATTLE_SMOKE_RESULT failures=0' -or $log -match 'BATTLE_TEST FAIL'){throw "Basic regression failed: $step"}
}
$games=@()
foreach($name in @('stages4-D-s7-r2','stages4-D-s7-swap-r2','stages4-A-s7-r2')){
 $dir=Join-Path $bench "SightTrial-20261111/$name"
 $m=Read-Json (Join-Path $dir 'metrics.json');$log=Get-Content (Join-Path $dir 'run.log') -Raw
 if(!$m.mixed_save_round_trip_verified -or $log -notmatch 'SIGHT_EXPERIMENT_RESULT failures=0'){throw "Incomplete game $name"}
 if($m.completed_replacements -ne 200-$m.ally_resources-$m.enemy_resources){throw "Resource conservation mismatch $name"}
 $allyPoints=@($m.final_objectives|Where-Object{$_.owner -eq 0}).Count
 $enemyPoints=@($m.final_objectives|Where-Object{$_.owner -eq 1}).Count
 if($m.ally_score -ne $m.ally_alive+$m.ally_resources+20*$allyPoints -or $m.enemy_score -ne $m.enemy_alive+$m.enemy_resources+20*$enemyPoints){throw "Final score mismatch $name"}
 $games+=@{name=$name;seconds=$m.match_seconds;winner=$m.winner;firstSight=$m.first_sight_seconds;firstShot=$m.first_shot_seconds;shots=$m.shots;points=$m.final_objectives;coverStarts=([regex]::Matches($log,'BATTLE_COVER_ACTION')).Count;supportHandoffs=([regex]::Matches($log,'BATTLE_FLANK_HANDOFF')).Count}
}
@{verified_at_utc=[DateTime]::UtcNow.ToString('o');passed=$true;dll=$latest.dll;sourceArchiveSha256=$latest.sourceArchiveSha256;cases=$verified;games=$games;sourceMismatches=0;preservedOriginalMaps=$snapshots['1'].maps.Count}|ConvertTo-Json -Depth 10|Set-Content (Join-Path $bench 'FlankStages-20261112/verification-final.json') -Encoding utf8
$games|ConvertTo-Json -Depth 8
