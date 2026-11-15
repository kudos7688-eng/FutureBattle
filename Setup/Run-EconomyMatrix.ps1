# Area economy comparison (2026-11-14): new-squad hiring x resource income, each ON/OFF. Runs Test-WideSight.ps1 for each
# against one frozen build. Existing output folders are skipped, so an interrupted matrix can be resumed by re-running
# the same command. Summarise with Setup/Analyze-EconomyMatrix.ps1.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$FrozenBuild,   # e.g. Benchmarks/BuildSnapshots/claude-supply-r1/manifest.json
 [string[]]$Conditions=@('HI','H','I','none'),
 [int[]]$Seeds=@(7,19,43),
 [string[]]$Maps=@('D','A'),
 [ValidateRange(1,16)][int]$Concurrency=12,
 [string]$Tag='r1',
 [ValidateRange(-1,1)][int]$TrialTeam=-1,   # 0/1: the S3-S6 trial flags apply to that team only (use a distinct -Tag)
 [ValidateRange(10,120)][int]$Fps=60,       # fixed simulation step (2026-11-16: 30 = about twice as fast; compare only runs with the same Fps)
 [switch]$NoRoundTripSave                   # skip the 80 s save round trip (strategy screening only; regressions keep it)
)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$frozen=(Resolve-Path $FrozenBuild).Path
# H = hiring, I = income; letters present are ON (Config default), absent ones are passed =0.
$supply=@{
 'HI'=@()
 'H'=@('-AreaIncome','0')
 'I'=@('-AreaHire','0')
 'none'=@('-AreaHire','0','-AreaIncome','0')
 # E2 AI hiring pace on top of HI: S = stop buying N s before the time limit, G = minimum gap between buys (s).
 'S120'=@('-AreaHireStop','120')
 'S180'=@('-AreaHireStop','180')
 'G60'=@('-AreaHireInterval','60')
 # AI strategy S1 (weighted point spread, fixed per-squad slot): W = weight of points not observed as own.
 'S1'=@('-AreaStrategy','1')
 'S1W3'=@('-AreaStrategy','1','-AreaAttackWeight','3')
 'S1B0'=@('-AreaStrategy','1','-AreaBaseShare','0')
 # S3 conditional home assault on top of the default S1: High share with 2+ observed own points or seen enemy home HP <= 500.
 'B3'=@('-AreaBaseRule','1')
 'B3L1'=@('-AreaBaseRule','1','-AreaBaseShareLow','0.1')
 'B3H4'=@('-AreaBaseRule','1','-AreaBaseShareHigh','0.4')
 # S4 front balance on top of the default S1: weight x (1 + gain x own-soldier deficit near the point).
 'F1'=@('-AreaFrontBalance','1')
 'F2'=@('-AreaFrontBalance','1','-AreaFrontGain','2')
 'F05'=@('-AreaFrontBalance','1','-AreaFrontGain','0.5')
 # Numeric tuning: AI hire reserve (HR0; R0 was renamed when the range-profile R0 arrived with the merge) and S1 attack weight.
 'HR0'=@('-AreaHireReserve','0')
 'R25'=@('-AreaHireReserve','25')
 'W15'=@('-AreaAttackWeight','1.5')
 # S5 home-assault infiltration: Move (no stopping to fight) until the leader is within N m of the enemy home.
 'IN60'=@('-AreaBaseInfiltrate','60')
 'IN100'=@('-AreaBaseInfiltrate','100')
 'IN150'=@('-AreaBaseInfiltrate','150')
 # S1 home-assault share (default 0.2) numeric tuning.
 'BS10'=@('-AreaBaseShare','0.1')
 'BS30'=@('-AreaBaseShare','0.3')
 # Home supply heal numeric tuning (defaults: heal 10/s, resume at 90% squad HP).
 'SH20'=@('-SupplyHeal','20')
 'RH75'=@('-SupplyResumeHealth','0.75')
 'RH100'=@('-SupplyResumeHealth','1.0')
 # S6 focus: the non-own point nearest the own home gets weight x (1+gain).
 'FO1'=@('-AreaFocus','1')
 'FO2'=@('-AreaFocus','2')
 'F1FO1'=@('-AreaFrontBalance','1','-AreaFocus','1')
 # S7 flank route for home-assault squads: waypoint at y = +/-f x side-point |y| before heading for the enemy home.
 'FL13'=@('-AreaBaseFlank','1.3')
 'FL10'=@('-AreaBaseFlank','1.0')
 # S8 concentration: home-assault squads return for supply at 5 living (BR5) and/or all take the same flank side (FS13).
 'BR5'=@('-AreaBaseReturnLiving','5')
 'FS13'=@('-AreaBaseFlank','1.3','-AreaBaseFlankSide','1')
 'FS13BR5'=@('-AreaBaseFlank','1.3','-AreaBaseFlankSide','1','-AreaBaseReturnLiving','5')
 # Since 2026-11-15 HI includes S4+S6 (Config). S1only = the previous default (S4 and S6 off).
 'S1only'=@('-AreaFrontBalance','0','-AreaFocus','0')
 # Income-curve trials (2026-11-16): INC2 doubles base and per-point income; RAMP2 multiplies each payment by 1+0.2 x minutes;
 # RAMP2S50 also halves the starting resources (similar total, shifted toward the late game).
 'INC1'=@('-AreaIncomeBase','1','-AreaIncomePerPoint','1')
 'INC2'=@('-AreaIncomeBase','2','-AreaIncomePerPoint','2')
 'INC3'=@('-AreaIncomeBase','3','-AreaIncomePerPoint','3')
 'INC4'=@('-AreaIncomeBase','4','-AreaIncomePerPoint','4')
 'INC5'=@('-AreaIncomeBase','5','-AreaIncomePerPoint','5')
 'RAMP2'=@('-AreaIncomeRamp','0.2')
 'RAMP2S50'=@('-AreaIncomeRamp','0.2','-AreaStartCredits','50')
 # P1 purpose purchase: the AI buys only for an observed need (home under attack, thin point, weak enemy home) and keeps that mission.
 'P1'=@('-AreaHirePurpose','1')
 'P1RAMP'=@('-AreaHirePurpose','1','-AreaIncomeRamp','0.2','-AreaStartCredits','50')
 # Strategy layer stage 2 plans (time phases): EH expand then hold, ESA expand, save, then mass on the enemy home.
 'EH'=@('-AreaPlan','1')
 'ESA'=@('-AreaPlan','2')
 # Suppression trial: P = defaults (spread x2.5 at full, pin at 0.5); PS = spread only (no pinning); PP = pinning only; PH = pin at 0.75.
 'P'=@('-Suppression','1')
 'PS'=@('-Suppression','1','-SuppressionPin','2')
 'PP'=@('-Suppression','1','-SuppressionSpread','0')
 'PH'=@('-Suppression','1','-SuppressionPin','0.75')
 # War score trial: W = time limit only; W400 = also ends when a team's earned score reaches 400.
 'W'=@('-AreaWarScore','1')
 'W400'=@('-AreaWarScore','1','-AreaWarTarget','400')
 # Range profile (map E default): R0 = off (100m sight / 50m range as on A-D); RE75/RE100/RE125 = carbine AI engagement distance;
 # RS150 = 150m sight; RP = profile with the suppression trial; RN = profile with 100m sight (ranges only).
 'R0'=@('-RangeProfile','0')
 'RE75'=@('-RangeEngage','7500')
 'RE100'=@('-RangeEngage','10000')
 'RE125'=@('-RangeEngage','12500')
 'RS150'=@('-RangeSight','15000')
 'RN'=@('-RangeSight','10000')
 'RP'=@('-Suppression','1')
 # Stances (map E default ON): T0 = off; TP = stances with the suppression trial (pinned soldiers go prone).
 'T0'=@('-Stances','0')
 'TP'=@('-Suppression','1')
 'T0P'=@('-Stances','0','-Suppression','1')
 # CA0 = map E without cover-first arrival slots (members arrive on the open grid as on maps A-D).
 'CA0'=@('-CoverArrival','0')
 # TP3 = stances with stronger suppression per shot (0.3 instead of 0.12).
 'TP3'=@('-Suppression','1','-SuppressionPerShot','0.3')
}
$runs=@()
foreach($c in $Conditions){ if(!$supply.ContainsKey($c)){throw "Unknown condition $c"}
 foreach($seed in $Seeds){ foreach($map in $Maps){ foreach($swap in $false,$true){ $runs+=,@($c,$seed,$map,$swap) } } } }
$jobs=@(); $skipped=0
foreach($r in $runs){
 $c,$seed,$map,$swap=$r
 $name="econ-$c-$map-s$seed"+($(if($swap){'-swap'}else{''}))+"-$Tag"
 if(Test-Path "Benchmarks/SightTrial-20261111/$name"){ $skipped++; continue }
 $pa=@('-NoProfile','-ExecutionPolicy','Bypass','-File','.\Test-WideSight.ps1','-Map',$map,'-OutputName',$name,'-Seed',"$seed",
  '-AICoverProbe','0','-LeaderGuard','0','-SightRange','10000','-WeaponRange','5000','-Area','-Experiment',
  '-CoverActions','1','-FlankCoordination','1','-ObserveTactics','-BehaviorConcurrency',"$Concurrency",'-FrozenBuild',$frozen)+$supply[$c]
 if($TrialTeam -ge 0){$pa+=@('-AreaTrialTeam',"$TrialTeam")}
 if(!$NoRoundTripSave){$pa+='-RoundTripSave'}
 if($Fps -ne 60){$pa+=@('-Fps',"$Fps")}
 if($swap){$pa+='-Swap'}
 while(@($jobs|Where-Object{!$_.HasExited}).Count -ge $Concurrency){Start-Sleep 5}
 $jobs+=Start-Process powershell.exe -ArgumentList $pa -RedirectStandardOutput "Econ-$name.txt" -RedirectStandardError "Econ-$name.err.txt" -WindowStyle Hidden -PassThru
 Start-Sleep 3
}
$jobs|ForEach-Object{$_.WaitForExit()}
"started=$($jobs.Count) skipped(existing)=$skipped"
