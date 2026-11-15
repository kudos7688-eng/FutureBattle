[CmdletBinding()]
param([ValidateSet('A','B','C','D')][string]$Map='A',[string]$OutputName='audit-A',[switch]$Rendered,[switch]$Experiment,[switch]$Visual,[switch]$FirstPerson,[switch]$GeometryOnly,[ValidateRange(0,10000)][int]$WeaponRange=0,[int]$SightRange=5000,[int]$Seed=7,[switch]$Swap,[ValidateRange(1,16)][int]$BehaviorConcurrency=1,[string]$FrozenBuild='',[ValidateSet(0,1)][int]$FlankEnabled=1,[switch]$ObserveTactics,[switch]$RoundTripSave,[switch]$Area,[ValidateSet(0,1)][int]$FlankRelaxed=0,[ValidateRange(0,3)][int]$FlankTeams=3,[switch]$FlankTrial,[ValidateSet(0,1)][int]$CoverActions=0,[ValidateSet(0,1)][int]$FlankCoordination=0,[ValidateSet(0,1)][int]$AICoverProbe=0,[ValidateSet(0,1)][int]$CoverApproach=0,[ValidateSet(-1,0,1)][int]$ReacquireGrace=-1,[ValidateSet(-1,0,1)][int]$CoverFire=-1,[ValidateSet(-1,0,1)][int]$CoverValidation=-1,[ValidateSet(-1,0,1)][int]$CoverPeekSight=-1,[ValidateSet(0,1)][int]$PerceptionOpt=0,[ValidateSet(0,1)][int]$LeaderGuard=0,[ValidateRange(0,10)][int]$LeaderHealth=0,[int]$SupplyReturnLiving=-1,[int]$SupplyResumeLiving=-1,[double]$SupplyAwayRatio=-1,[double]$SupplyHeal=-1,[double]$SupplyResumeHealth=-1,[ValidateSet(-1,0,1)][int]$AreaHire=-1,[ValidateSet(-1,0,1)][int]$AreaIncome=-1,[int]$AreaHireReserve=-1,[double]$AreaHireInterval=-1,[double]$AreaHireStop=-1,[ValidateSet(-1,0,1)][int]$AreaStrategy=-1,[double]$AreaAttackWeight=-1,[double]$AreaBaseShare=-1,[ValidateSet(-1,0,1)][int]$AreaBaseRule=-1,[double]$AreaBaseShareLow=-1,[double]$AreaBaseShareHigh=-1,[ValidateSet(-1,0,1)][int]$AreaFrontBalance=-1,[double]$AreaFrontGain=-1,[double]$AreaBaseInfiltrate=-1,[double]$AreaFocus=-1,[double]$AreaBaseFlank=-1,[int]$AreaBaseFlankSide=-1,[int]$AreaBaseReturnLiving=-1,[int]$AreaTrialTeam=-1,[int]$AreaIncomeBase=-1,[int]$AreaIncomePerPoint=-1,[double]$AreaIncomeRamp=-1,[int]$AreaStartCredits=-1,[int]$AreaHirePurpose=-1)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$out=Join-Path $PSScriptRoot ('Benchmarks/SightTrial-20261111/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out | Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$mapFile=Join-Path $root "Game/FutureBattle/Content/Prototype/Lvl_BattleWide_$Map.umap"
$hashes=@()
if(!$FrozenBuild){$hashes=@(Get-ChildItem (Join-Path $root 'Game/FutureBattle/Source'),(Join-Path $root 'Game/FutureBattle/Config') -File -Recurse | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})}
$snapshot=$null
if($FrozenBuild){
 $snapshot=Get-Content -LiteralPath $FrozenBuild -Raw|ConvertFrom-Json
 if((Get-FileHash $dll).Hash -ne $snapshot.dll -or (Get-FileHash $snapshot.sourceArchive).Hash -ne $snapshot.sourceArchiveSha256){throw 'Frozen build or source archive changed'}
 foreach($file in $snapshot.files){if($file.path -notmatch '[\\/]Source[\\/]' -and (Get-FileHash $file.path).Hash -ne $file.sha256){throw 'Runtime configuration changed since snapshot'}}
 $expectedMap=@($snapshot.maps|Where-Object{$_.path -eq $mapFile})
 if($expectedMap.Count -ne 1 -or (Get-FileHash $mapFile).Hash -ne $expectedMap[0].sha256){throw 'Map changed since snapshot'}
 # C++ edits may continue, but this run executes the pinned DLL and unchanged runtime configuration.
 $hashes=$snapshot.files
}
$savedSettings=@(Get-ChildItem (Join-Path $root 'Game/FutureBattle/Saved/Config/WindowsEditor') -Filter '*.ini' -File | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
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
if($AreaHirePurpose -ge 0){$a+="-BattleAreaHirePurpose=$AreaHirePurpose"};if($AreaIncomeBase -ge 0){$a+="-BattleAreaIncomeBase=$AreaIncomeBase"};if($AreaIncomePerPoint -ge 0){$a+="-BattleAreaIncomePerPoint=$AreaIncomePerPoint"};if($AreaIncomeRamp -ge 0){$a+=("-BattleAreaIncomeRamp="+$AreaIncomeRamp.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaStartCredits -ge 0){$a+="-BattleAreaStartCredits=$AreaStartCredits"};if($AreaBaseFlankSide -ge 0){$a+="-BattleAreaBaseFlankSide=$AreaBaseFlankSide"};if($AreaBaseReturnLiving -ge 0){$a+="-BattleAreaBaseReturnLiving=$AreaBaseReturnLiving"};if($AreaBaseFlank -ge 0){$a+=("-BattleAreaBaseFlank="+$AreaBaseFlank.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaTrialTeam -ge 0){$a+="-BattleAreaTrialTeam=$AreaTrialTeam"};if($AreaFocus -ge 0){$a+=("-BattleAreaFocus="+$AreaFocus.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseInfiltrate -ge 0){$a+=("-BattleAreaBaseInfiltrate="+$AreaBaseInfiltrate.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaFrontBalance -ge 0){$a+="-BattleAreaFrontBalance=$AreaFrontBalance"};if($AreaFrontGain -ge 0){$a+=("-BattleAreaFrontGain="+$AreaFrontGain.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaHireReserve -ge 0){$a+="-BattleAreaHireReserve=$AreaHireReserve"};if($AreaHireInterval -ge 0){$a+=("-BattleAreaHireInterval="+$AreaHireInterval.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaHireStop -ge 0){$a+=("-BattleAreaHireStop="+$AreaHireStop.ToString([Globalization.CultureInfo]::InvariantCulture))}
$a+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams")
if($FlankTrial){$a+='-BattleFlankTrial'}
if($ObserveTactics){$a+='-BattleTacticalObserve'}
if($Area){$a+='-BattleArea'}
if($RoundTripSave){if(!$Experiment){throw 'RoundTripSave requires Experiment'};$a+='-BattleVerifyMixedSave'}
if($WeaponRange){$a+="-BattleWeaponRange=$WeaponRange"}
if($Swap -and !$Experiment){$a+='-BattleTrialSwap'}
if($Visual){$a+='-BattleSightVisual';if($FirstPerson){$a+='-BattleFirstPerson'}}elseif($Experiment){$a+=@(('-BattleSightExperiment="'+(Join-Path $out 'metrics.json')+'"'),"-BattleTrialSeed=$Seed",'-UseFixedTimeStep','-FPS=60');if($Swap){$a+='-BattleTrialSwap'}}else{$a+='-BattleWideMapAudit'}
if($GeometryOnly){$a+='-BattleWideMapGeometryOnly'}
if($Rendered){$a+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$a+='-nullrhi'}
if($Rendered -and $BehaviorConcurrency -ne 1){throw 'Rendered measurements must run alone'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;map=(Get-FileHash $mapFile).Hash;files=$hashes;source_snapshot=$snapshot;savedSettings=$savedSettings;arguments=$a;behaviorConcurrency=$BehaviorConcurrency;timingComparable=($BehaviorConcurrency -eq 1)} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
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

