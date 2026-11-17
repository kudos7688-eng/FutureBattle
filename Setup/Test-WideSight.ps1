[CmdletBinding()]
param([ValidateSet('A','B','C','D','E')][string]$Map='A',[string]$OutputName='audit-A',[switch]$Rendered,[switch]$Experiment,[switch]$Visual,[switch]$FirstPerson,[switch]$GeometryOnly,[ValidateRange(0,10000)][int]$WeaponRange=0,[int]$SightRange=5000,[int]$Seed=7,[switch]$Swap,[ValidateRange(1,16)][int]$BehaviorConcurrency=1,[string]$FrozenBuild='',[ValidateSet(0,1)][int]$FlankEnabled=1,[switch]$ObserveTactics,[switch]$RoundTripSave,[switch]$Area,[ValidateSet(0,1)][int]$FlankRelaxed=0,[ValidateRange(0,3)][int]$FlankTeams=3,[switch]$FlankTrial,[ValidateSet(0,1)][int]$CoverActions=0,[ValidateSet(0,1)][int]$FlankCoordination=0,[ValidateSet(0,1)][int]$AICoverProbe=0,[ValidateSet(0,1)][int]$CoverApproach=0,[ValidateSet(-1,0,1)][int]$ReacquireGrace=-1,[ValidateSet(-1,0,1)][int]$CoverFire=-1,[ValidateSet(-1,0,1)][int]$CoverValidation=-1,[ValidateSet(-1,0,1)][int]$CoverPeekSight=-1,[ValidateSet(0,1)][int]$PerceptionOpt=0,[ValidateSet(0,1)][int]$LeaderGuard=0,[ValidateRange(0,10)][int]$LeaderHealth=0,[int]$SupplyReturnLiving=-1,[int]$SupplyResumeLiving=-1,[double]$SupplyAwayRatio=-1,[double]$SupplyHeal=-1,[double]$SupplyResumeHealth=-1,[ValidateSet(-1,0,1)][int]$AreaHire=-1,[ValidateSet(-1,0,1)][int]$AreaIncome=-1,[int]$AreaHireReserve=-1,[double]$AreaHireInterval=-1,[double]$AreaHireStop=-1,[ValidateSet(-1,0,1)][int]$AreaStrategy=-1,[double]$AreaAttackWeight=-1,[double]$AreaBaseShare=-1,[ValidateSet(-1,0,1)][int]$AreaBaseRule=-1,[double]$AreaBaseShareLow=-1,[double]$AreaBaseShareHigh=-1,[ValidateSet(-1,0,1)][int]$AreaFrontBalance=-1,[double]$AreaFrontGain=-1,[double]$AreaBaseInfiltrate=-1,[double]$AreaFocus=-1,[double]$AreaBaseFlank=-1,[int]$AreaBaseFlankSide=-1,[int]$AreaBaseReturnLiving=-1,[int]$AreaTrialTeam=-1,[int]$AreaIncomeBase=-1,[int]$AreaIncomePerPoint=-1,[double]$AreaIncomeRamp=-1,[int]$AreaStartCredits=-1,[int]$AreaHirePurpose=-1,[int]$AreaPlan=-1,[double]$AreaHeavyShare=-1,[ValidateSet(-1,0,1)][int]$ReturnFire=-1,[int]$AreaIncomeUrbanBase=-1,[int]$AreaIncomeUrbanPerPoint=-1,[ValidateRange(10,120)][int]$Fps=60,[ValidateSet(-1,0,1)][int]$Suppression=-1,[double]$SuppressionPin=-1,[double]$SuppressionSpread=-1,[double]$SuppressionPerShot=-1,[double]$SuppressionSupport=-1,[ValidateSet(-1,0,1)][int]$AreaWarScore=-1,[int]$AreaWarTarget=-1,[ValidateSet(-1,0,1)][int]$RangeProfile=-1,[int]$RangeSight=-1,[int]$RangeEngage=-1,[int]$RangeEngageSupport=-1,[int]$RangeReach=-1,[ValidateSet(-1,0,1)][int]$Stances=-1,[ValidateSet(-1,0,1)][int]$CoverArrival=-1,[int]$HeavyMembers=-1,[double]$HeavyHealth=-1,[double]$HeavySpeed=-1,[int]$HeavyCost=-1,[double]$HeavyDamage=-1,[int]$AreaScoutKeep=-1,[ValidateSet(-1,0,1)][int]$AreaIntel=-1,[ValidateSet(-1,0,1)][int]$ScoutRetreat=-1,[int]$AreaATKeep=-1,[int]$AreaTankKeep=-1,[int]$AreaACKeep=-1,[int]$AreaMortarKeep=-1,[int]$AreaSPGKeep=-1,[ValidateSet(-1,0,1)][int]$ArmourAI=-1,[ValidateSet(-1,0,1)][int]$ArmourAITeam=-1,[ValidateSet(-1,0,1)][int]$AreaComposition=-1,[ValidateSet(-1,0,1)][int]$AreaGroups=-1,[string]$Trace='',[ValidateSet(-1,0,1)][int]$SightPairReuse=-1,[double]$SightScanInterval=-1,[ValidateSet(-1,0,1)][int]$SightHitStats=-1,[ValidateSet(-1,0,1)][int]$SightParallel=-1,[ValidateSet(-1,0,1)][int]$SightSlowFar=-1,[ValidateSet(-1,0,1)][int]$SightTeamMask=-1,[ValidateSet(-1,0,1)][int]$HireSightRefresh=-1,[switch]$ReuseIdentical)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$snapshot=$null
if($FrozenBuild){$snapshot=Get-Content -LiteralPath $FrozenBuild -Raw|ConvertFrom-Json}
# 2026-11-18: snapshots made by New-BattleBuildSnapshot.ps1 carry their own project copy; UE then runs from that copy and
# the checks below look at the copied DLL/Config/map, so builds and Config edits in the working tree do not affect the run.
$projDir=if($snapshot -and $snapshot.projectCopy){$snapshot.projectCopy}else{Join-Path $root 'Game/FutureBattle'}
$project=Join-Path $projDir 'FutureBattle.uproject'
$out=Join-Path $PSScriptRoot ('Benchmarks/SightTrial-20261111/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out | Out-Null
$dll=Join-Path $projDir 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$mapFile=Join-Path $projDir "Content/Prototype/Lvl_BattleWide_$Map.umap"
$hashes=@()
if(!$FrozenBuild){$hashes=@(Get-ChildItem (Join-Path $root 'Game/FutureBattle/Source'),(Join-Path $root 'Game/FutureBattle/Config') -File -Recurse | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})}
if($FrozenBuild){
 if((Get-FileHash $dll).Hash -ne $snapshot.dll -or (Get-FileHash $snapshot.sourceArchive).Hash -ne $snapshot.sourceArchiveSha256){throw 'Frozen build or source archive changed'}
 foreach($file in $snapshot.files){if($file.path -notmatch '[\\/]Source[\\/]' -and (Get-FileHash $file.path).Hash -ne $file.sha256){throw 'Runtime configuration changed since snapshot'}}
 $expectedMap=@($snapshot.maps|Where-Object{$_.path -eq $mapFile})
 if($expectedMap.Count -ne 1 -or (Get-FileHash $mapFile).Hash -ne $expectedMap[0].sha256){throw 'Map changed since snapshot'}
 # C++ edits may continue, but this run executes the pinned DLL and unchanged runtime configuration.
 $hashes=$snapshot.files
}
$savedSettings=@(Get-ChildItem (Join-Path $projDir 'Saved/Config/WindowsEditor') -Filter '*.ini' -File | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleRangeAware=1',"-BattleSightRange=$SightRange",('-abslog="'+$log+'"'))
$a+=@("-BattleSquadFlank=$FlankEnabled","-BattleCoverActions=$CoverActions","-BattleFlankCoordination=$FlankCoordination","-BattleAICoverProbe=$AICoverProbe","-BattleCoverApproach=$CoverApproach","-BattleLeaderGuard=$LeaderGuard")
if($LeaderHealth -gt 0){$a+="-BattleLeaderHealth=$LeaderHealth"}
# Supply trials: -1 = not passed (Config default).
if($SupplyReturnLiving -ge 0){$a+="-BattleSupplyReturnLiving=$SupplyReturnLiving"};if($SupplyResumeLiving -ge 0){$a+="-BattleSupplyResumeLiving=$SupplyResumeLiving"}
if($SupplyAwayRatio -ge 0){$a+=("-BattleSupplyAwayRatio="+$SupplyAwayRatio.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SupplyHeal -ge 0){$a+=("-BattleSupplyHeal="+$SupplyHeal.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($SupplyResumeHealth -ge 0){$a+=("-BattleSupplyResumeHealth="+$SupplyResumeHealth.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($ReacquireGrace -ge 0){$a+="-BattleReacquireGrace=$ReacquireGrace"}
if($CoverFire -ge 0){$a+="-BattleCoverFire=$CoverFire"}
if($CoverValidation -ge 0){$a+="-BattleCoverValidation=$CoverValidation"}
if($CoverPeekSight -ge 0){$a+="-BattleCoverPeekSight=$CoverPeekSight"}
if($PerceptionOpt -eq 1){$a+='-BattleReuseSightContext=1','-BattlePerceptionGrid=1','-BattleReduceHiddenBones=1'}
if($AreaHire -ge 0){$a+="-BattleAreaHire=$AreaHire"}
if($AreaIncome -ge 0){$a+="-BattleAreaIncome=$AreaIncome"}
if($AreaStrategy -ge 0){$a+="-BattleAreaStrategy=$AreaStrategy"};if($AreaAttackWeight -ge 0){$a+=("-BattleAreaAttackWeight="+$AreaAttackWeight.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseShare -ge 0){$a+=("-BattleAreaBaseShare="+$AreaBaseShare.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaBaseRule -ge 0){$a+="-BattleAreaBaseRule=$AreaBaseRule"};if($AreaBaseShareLow -ge 0){$a+=("-BattleAreaBaseShareLow="+$AreaBaseShareLow.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseShareHigh -ge 0){$a+=("-BattleAreaBaseShareHigh="+$AreaBaseShareHigh.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaHeavyShare -ge 0){$a+=("-BattleAreaHeavyShare="+$AreaHeavyShare.ToString([Globalization.CultureInfo]::InvariantCulture))};if($ReturnFire -ge 0){$a+="-BattleReturnFire=$ReturnFire"};if($AreaIncomeUrbanBase -ge 0){$a+="-BattleAreaIncomeUrbanBase=$AreaIncomeUrbanBase"};if($AreaIncomeUrbanPerPoint -ge 0){$a+="-BattleAreaIncomeUrbanPerPoint=$AreaIncomeUrbanPerPoint"};if($AreaPlan -ge 0){$a+="-BattleAreaPlan=$AreaPlan"};if($AreaHirePurpose -ge 0){$a+="-BattleAreaHirePurpose=$AreaHirePurpose"};if($AreaIncomeBase -ge 0){$a+="-BattleAreaIncomeBase=$AreaIncomeBase"};if($AreaIncomePerPoint -ge 0){$a+="-BattleAreaIncomePerPoint=$AreaIncomePerPoint"};if($AreaIncomeRamp -ge 0){$a+=("-BattleAreaIncomeRamp="+$AreaIncomeRamp.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaStartCredits -ge 0){$a+="-BattleAreaStartCredits=$AreaStartCredits"};if($AreaBaseFlankSide -ge 0){$a+="-BattleAreaBaseFlankSide=$AreaBaseFlankSide"};if($AreaBaseReturnLiving -ge 0){$a+="-BattleAreaBaseReturnLiving=$AreaBaseReturnLiving"};if($AreaBaseFlank -ge 0){$a+=("-BattleAreaBaseFlank="+$AreaBaseFlank.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaTrialTeam -ge 0){$a+="-BattleAreaTrialTeam=$AreaTrialTeam"};if($AreaFocus -ge 0){$a+=("-BattleAreaFocus="+$AreaFocus.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseInfiltrate -ge 0){$a+=("-BattleAreaBaseInfiltrate="+$AreaBaseInfiltrate.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaFrontBalance -ge 0){$a+="-BattleAreaFrontBalance=$AreaFrontBalance"};if($AreaFrontGain -ge 0){$a+=("-BattleAreaFrontGain="+$AreaFrontGain.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaHireReserve -ge 0){$a+="-BattleAreaHireReserve=$AreaHireReserve"};if($AreaHireInterval -ge 0){$a+=("-BattleAreaHireInterval="+$AreaHireInterval.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaHireStop -ge 0){$a+=("-BattleAreaHireStop="+$AreaHireStop.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaWarScore -ge 0){$a+="-BattleAreaWarScore=$AreaWarScore"};if($AreaWarTarget -ge 0){$a+="-BattleAreaWarTarget=$AreaWarTarget"}
# Range profile (map E default ON; see BattleRanges.h): -1 = not passed. Values in cm.
if($RangeProfile -ge 0){$a+="-BattleRangeProfile=$RangeProfile"};if($RangeSight -ge 0){$a+="-BattleRangeSight=$RangeSight"};if($RangeEngage -ge 0){$a+="-BattleRangeEngage=$RangeEngage"}
if($RangeEngageSupport -ge 0){$a+="-BattleRangeEngageSupport=$RangeEngageSupport"};if($RangeReach -ge 0){$a+="-BattleRangeReach=$RangeReach"}
if($Stances -ge 0){$a+="-BattleStances=$Stances"};if($CoverArrival -ge 0){$a+="-BattleCoverArrival=$CoverArrival"}
# Suppression trial: -1 = not passed (default OFF).
if($SuppressionSupport -ge 0){$a+=("-BattleSuppressionSupport="+$SuppressionSupport.ToString([Globalization.CultureInfo]::InvariantCulture))};if($Suppression -ge 0){$a+="-BattleSuppression=$Suppression"};if($SuppressionPin -ge 0){$a+=("-BattleSuppressionPin="+$SuppressionPin.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SuppressionSpread -ge 0){$a+=("-BattleSuppressionSpread="+$SuppressionSpread.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SuppressionPerShot -ge 0){$a+=("-BattleSuppressionPerShot="+$SuppressionPerShot.ToString([Globalization.CultureInfo]::InvariantCulture))}
# Scout pairs kept by the AI (trial team only with -AreaTrialTeam); -1 = not passed (default AreaScoutKeep).
if($AreaScoutKeep -ge 0){$a+="-BattleAreaScoutKeep=$AreaScoutKeep"}
# Intel trial (AI acts on known enemy positions) and scout retreat switch; -1 = not passed.
if($AreaIntel -ge 0){$a+="-BattleAreaIntel=$AreaIntel"};if($AreaATKeep -ge 0){$a+="-BattleAreaATKeep=$AreaATKeep"};if($AreaTankKeep -ge 0){$a+="-BattleAreaTankKeep=$AreaTankKeep"};if($AreaACKeep -ge 0){$a+="-BattleAreaACKeep=$AreaACKeep"};if($AreaMortarKeep -ge 0){$a+="-BattleAreaMortarKeep=$AreaMortarKeep"};if($AreaSPGKeep -ge 0){$a+="-BattleAreaSPGKeep=$AreaSPGKeep"};if($ArmourAI -ge 0){$a+="-BattleArmourAI=$ArmourAI"};if($ArmourAITeam -ge 0){$a+="-BattleArmourAITeam=$ArmourAITeam"};if($AreaComposition -ge 0){$a+="-BattleAreaComposition=$AreaComposition"};if($AreaGroups -ge 0){$a+="-BattleAreaGroups=$AreaGroups"};if($ScoutRetreat -ge 0){$a+="-BattleScoutRetreat=$ScoutRetreat"}
# Heavy infantry number trials (2026-11-16): override members, health and speed scale, purchase cost and damage per shot.
foreach($hv in @(@("Members",$HeavyMembers),@("Health",$HeavyHealth),@("Speed",$HeavySpeed),@("Cost",$HeavyCost),@("Damage",$HeavyDamage))){if($hv[1] -ge 0){$a+=("-BattleHeavy"+$hv[0]+"="+([double]$hv[1]).ToString([Globalization.CultureInfo]::InvariantCulture))}}
$a+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams")
if($FlankTrial){$a+='-BattleFlankTrial'}
if($ObserveTactics){$a+='-BattleTacticalObserve'}
if($Area){$a+='-BattleArea'}
if($RoundTripSave){if(!$Experiment){throw 'RoundTripSave requires Experiment'};$a+='-BattleVerifyMixedSave'}
if($WeaponRange){$a+="-BattleWeaponRange=$WeaponRange"}
if($Swap -and !$Experiment){$a+='-BattleTrialSwap'}
if($Visual){$a+='-BattleSightVisual';if($FirstPerson){$a+='-BattleFirstPerson'}}elseif($Experiment){$a+=@(('-BattleSightExperiment="'+(Join-Path $out 'metrics.json')+'"'),"-BattleTrialSeed=$Seed",'-UseFixedTimeStep',"-FPS=$Fps");if($Swap){$a+='-BattleTrialSwap'}}else{$a+='-BattleWideMapAudit'}
if($GeometryOnly){$a+='-BattleWideMapGeometryOnly'}
if($Rendered){$a+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$a+='-nullrhi'}
# Unreal Insights trace (e.g. -Trace cpu,frame): written next to the run log; named CPU events for a game-thread breakdown.
if($Trace){if($Trace -notmatch '^[A-Za-z0-9,]+$'){throw 'Trace channels: letters, digits and commas'};$a+=@("-trace=$Trace",('-tracefile="'+(Join-Path $out 'trace.utrace')+'"'),'-statnamedevents')}
# Line-of-sight pair reuse trial (performance); -1 = not passed (default OFF).
if($SightPairReuse -ge 0){$a+="-BattleSightPairReuse=$SightPairReuse"}
if($SightHitStats -ge 0){$a+="-BattleSightHitStats=$SightHitStats"};if($SightParallel -ge 0){$a+="-BattleSightParallel=$SightParallel"};if($SightTeamMask -ge 0){$a+="-BattleSightTeamMask=$SightTeamMask"}
# Sight scan interval trial (seconds for observers that saw no enemy at their last scan); -1 = not passed (default 0.4 s).
if($SightScanInterval -ge 0){$a+=("-BattleSightScanInterval="+$SightScanInterval.ToString([Globalization.CultureInfo]::InvariantCulture))}
# Unknown far enemies on alternate scan pulses (trial, 2026-11-17); -1 = not passed (off).
if($SightSlowFar -ge 0){$a+="-BattleSightSlowFar=$SightSlowFar"}
# Full sight update right after a hire or scout handover (trial 2026-11-18); -1 = not passed (default 1 = on).
if($HireSightRefresh -ge 0){$a+="-BattleHireSightRefresh=$HireSightRefresh"}
if($Rendered -and $BehaviorConcurrency -ne 1){throw 'Rendered measurements must run alone'}
# Identical-run key (2026-11-18): DLL, map, runtime Config/.uproject contents and the game arguments without per-output
# paths. Written for every frozen-build NullRHI experiment so later runs with the same key can reuse its result.
$reuseKey=$null;$reusedFrom=$null
if($snapshot -and $Experiment -and !$Rendered -and !$Trace){
 $runtime=@($hashes|Where-Object{$_.path -notmatch '[\\/]Source[\\/]'}|ForEach-Object{($_.path -replace '^.*[\\/](Config[\\/].*|FutureBattle\.uproject)$','$1' -replace '\\','/')+'='+$_.sha256}|Sort-Object)
 $keyArgs=@($a|Select-Object -Skip 1|Where-Object{$_ -notmatch '^-(abslog|BattleSightExperiment|BattleWideScreens|tracefile)='})
 $keyText=(@('dll='+(Get-FileHash $dll).Hash,'map='+(Get-FileHash $mapFile).Hash)+$runtime+$keyArgs) -join "`n"
 $reuseKey=[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($keyText))) -replace '-',''
 Set-Content (Join-Path $out 'reuse-key.txt') $reuseKey -Encoding ascii
 if($ReuseIdentical){
  foreach($d in Get-ChildItem (Split-Path $out) -Directory){
   if($d.FullName -eq $out){continue}
   $k=Join-Path $d.FullName 'reuse-key.txt';$r=Join-Path $d.FullName 'result.json'
   if(!(Test-Path $k) -or !(Test-Path $r) -or !(Test-Path (Join-Path $d.FullName 'metrics.json'))){continue}
   if((Get-Content $k -Raw).Trim() -ne $reuseKey){continue}
   $prev=Get-Content $r -Raw|ConvertFrom-Json
   if($prev.passed -and !$prev.reusedFrom){$reusedFrom=$d.Name;break}
  }
 }
}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;map=(Get-FileHash $mapFile).Hash;files=$hashes;source_snapshot=$snapshot;savedSettings=$savedSettings;arguments=$a;behaviorConcurrency=$BehaviorConcurrency;timingComparable=(($BehaviorConcurrency -eq 1) -and !$reusedFrom);reuseKey=$reuseKey;reusedFrom=$reusedFrom} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
if($reusedFrom){
 # Same build, map, Config and arguments already played and passed: copy its outcome instead of playing again.
 $src=Join-Path (Split-Path $out) $reusedFrom
 Copy-Item (Join-Path $src 'metrics.json'),(Join-Path $src 'run.log') $out
 $prev=Get-Content (Join-Path $src 'result.json') -Raw|ConvertFrom-Json
 @{passed=$true;exitCode=$prev.exitCode;dll=$prev.dll;reusedFrom=$reusedFrom} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
 "REUSED identical run $reusedFrom"
 Get-Content $log | Where-Object {$_ -match 'WIDE_AUDIT|SIGHT_EXPERIMENT|SIGHT_VISUAL'} | Write-Output
 return
}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
$timeout=if($Experiment){2700000}else{210000}
if(!$p.WaitForExit($timeout)){Stop-Process -Id $p.Id;throw "Timeout: $log"}
$t=Get-Content $log -Raw
$pass=$p.ExitCode -eq 0 -and $t -match '(WIDE_AUDIT|SIGHT_EXPERIMENT|SIGHT_VISUAL)_RESULT failures=0' -and $t -notmatch 'WIDE_AUDIT_CHECK FAIL|Fatal error:|Assertion failed:'
if($RoundTripSave){$pass=$pass -and $t -match 'BATTLE_MIXED_SAVE_SOAK_RESULT failures=0'}
if($snapshot -and (Get-FileHash $dll).Hash -ne $snapshot.dll){$pass=$false}
if($snapshot){
 foreach($file in $snapshot.files){if($file.path -notmatch '[\\/]Source[\\/]' -and (Get-FileHash $file.path).Hash -ne $file.sha256){$pass=$false}}
 if((Get-FileHash $mapFile).Hash -ne $expectedMap[0].sha256 -or (Get-FileHash $snapshot.sourceArchive).Hash -ne $snapshot.sourceArchiveSha256){$pass=$false}
}
@{passed=$pass;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n" | Where-Object {$_ -match 'WIDE_AUDIT|SIGHT_EXPERIMENT|SIGHT_VISUAL'} | Write-Output
if(!$pass){throw "Failed: $log"}

