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
 [switch]$NoRoundTripSave,                  # skip the 80 s save round trip (strategy screening only; regressions keep it)
 [switch]$NoReuse,                          # 2026-11-18: by default a match whose build, map, Config and arguments equal an earlier passed one copies its result
 [switch]$PlanOnly                          # 2026-11-18: output the runs still missing ({Name, Args}) without starting them (Run-StrategyScreen pools them)
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
 # FS0 = the pre-2026-11-17 straight home assault (FS13BR5 is the default since then)
 'FS0'=@('-AreaBaseFlank','0','-AreaBaseFlankSide','0','-AreaBaseReturnLiving','0')
 # Since 2026-11-15 HI includes S4+S6 (Config). S1only = the previous default (S4 and S6 off).
 'S1only'=@('-AreaFrontBalance','0','-AreaFocus','0')
 # Income-curve trials (2026-11-16): INC2 doubles base and per-point income; RAMP2 multiplies each payment by 1+0.2 x minutes;
 # RAMP2S50 also halves the starting resources (similar total, shifted toward the late game).
 'INC1'=@('-AreaIncomeBase','1','-AreaIncomePerPoint','1')
 # Map E income candidates (2026-11-16): EI12 = 1 + 2 per point, EI22 = 2 + 2, EI23 = 2 + 3 (A-D keep INC4).
 'EI12'=@('-AreaIncomeUrbanBase','1','-AreaIncomeUrbanPerPoint','2')
 'EI22'=@('-AreaIncomeUrbanBase','2','-AreaIncomeUrbanPerPoint','2')
 'EI23'=@('-AreaIncomeUrbanBase','2','-AreaIncomeUrbanPerPoint','3')
 # Return fire: AI soldiers hit in the last 5 s may fire back at visible contacts up to bullet reach (matters on map E).
 'RF'=@('-ReturnFire','1')
 # Unknown far enemies on alternate scan pulses (2026-11-17, map E screen; with -TrialTeam only that team).
 'SF'=@('-SightSlowFar','1')
 # Heavy infantry: share of AI purchases that are heavy infantry (6 soldiers, support guns, 3x health; since 2026-11-16 18:10 1.5x damage and 15 resources, earlier runs 1x and 18).
 'HV33'=@('-AreaHeavyShare','0.33')
 'HV50'=@('-AreaHeavyShare','0.5')
 # HV0 = rifle squads only (the default before 2026-11-16 18:50, when the AI share became 0.33; HI now equals HV33).
 'HV0'=@('-AreaHeavyShare','0')
 # SC0 = no scout pairs (default since 2026-11-16 23:40: the AI keeps up to two).
 'SC0'=@('-AreaScoutKeep','0')
 # IN = intel trial (AI reinforces threatened points, attacks thinly held ones, assaults a weak home, buys defenders; known positions only).
 'IN'=@('-AreaIntel','1')
 # Weapon role: suppression on (both teams) with support-gun shots suppressing 2.5x; HV33PS adds a 33% heavy-infantry share.
 'PS25'=@('-Suppression','1','-SuppressionSupport','2.5')
 'HV33PS'=@('-Suppression','1','-SuppressionSupport','2.5','-AreaHeavyShare','0.33')
 # Heavy infantry numbers (33% share), screened against the old 1x/18 values (r27f): HD20 = 2x damage, HC12 = cost 12, HM8 = 8 soldiers, HD15C15 = 1.5x damage and cost 15 (adopted as default).
 'HD20'=@('-AreaHeavyShare','0.33','-HeavyDamage','2')
 'HC12'=@('-AreaHeavyShare','0.33','-HeavyCost','12')
 'HM8'=@('-AreaHeavyShare','0.33','-HeavyMembers','8')
 'HD15C15'=@('-AreaHeavyShare','0.33','-HeavyDamage','1.5','-HeavyCost','15')
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
 # Active squad limit (2026-11-19, -BattleAreaMaxSquads; default 25 = HI): MS32/MS40 let both teams hire up to 32/40 active squads.
 'MS32'=@('-AreaMaxSquads','32')
 'MS40'=@('-AreaMaxSquads','40')
 # Group lane (2026-11-21, maps H/I): FL0 = front line off (the earlier default), FL1 = front line + front army + gaps (the new default),
 # GF1 = group fire support without the front line, SG1 = front line + surprise group.
 'FL0'=@('-FrontLine','0')
 'FL1'=@('-FrontLine','1','-FrontAuto','1')
 # FLD = the default after ga1 (front line on, AI auto-fill off): should equal FL0 for AI teams.
 'FLD'=@('-FrontLine','1','-FrontAuto','0')
 'GF1'=@('-FrontLine','0','-GroupFireSupport','1')
 'SG1'=@('-FrontLine','1','-FrontAuto','1','-SurpriseGroup','1')
 # Strategy layer (2026-11-22, Docs/design-strategy-layer.md): SL1 = situation map log only (must equal the baseline);
 # ST2 = stage S2, recon -> focus attack (with -TrialTeam only that team, through -AreaTrialTeam).
 'SL1'=@('-SituationLog','1')
 'ST2'=@('-Strategy','2')
 'ST3'=@('-Strategy','3')
 'ST4'=@('-Strategy','4')
 # RTS = symmetric tie-break of the home-assault flank side (-BattleRouteTieSymmetric, 2026-11-22; maps E-I with the symmetric start).
 'RTS'=@('-RouteTieSymmetric','1')
 # Subtraction evaluation (2026-11-22, Docs/design-subtraction-eval.md): ALL = both teams buy every type (use as -Baseline);
 # W1..W9 = the same without one type (heavy, scout, AT, tank, armoured car, mortar, SPG, exo, supply; with -TrialTeam that team only).
 'ALL'=@('-AreaAllTypes','1')
 'W1'=@('-AreaAllTypes','1','-AreaWithout','1')
 'W2'=@('-AreaAllTypes','1','-AreaWithout','2')
 'W3'=@('-AreaAllTypes','1','-AreaWithout','3')
 'W4'=@('-AreaAllTypes','1','-AreaWithout','4')
 'W5'=@('-AreaAllTypes','1','-AreaWithout','5')
 'W6'=@('-AreaAllTypes','1','-AreaWithout','6')
 'W7'=@('-AreaAllTypes','1','-AreaWithout','7')
 'W8'=@('-AreaAllTypes','1','-AreaWithout','8')
 'W9'=@('-AreaAllTypes','1','-AreaWithout','9')
 # Front army share (2026-11-22, ROADMAP stage 5; maps H/I): FS50 / FS30 = the AI fills its front army up to half / 30% of its free infantry.
 'FS50'=@('-FrontShare','0.5')
 'FS30'=@('-FrontShare','0.3')
 # Stage 1 (2026-11-22): WP = corner waypoint pass (-BattleWaypointPass=1) on top of the default (HI).
 'WP'=@('-WaypointPass','1')
 # WP0 = the old movement rule (-BattleWaypointPass=0) after the waypoint pass became the default (2026-11-22).
 'WP0'=@('-WaypointPass','0')
 # TM = own side's soldiers do not block sight (-BattleSightTeamMask=1; the shot trace uses the same query, so they do not stop the side's bullets either).
 'TM'=@('-SightTeamMask','1')
 # SC = the sector AI combination the sector lane uses as its base (s22/s23) plus arrival walk (maps H/I).
 'SC'=@('-SectorContact','1','-SectorRoute','1','-SectorFlank','1','-SectorFront','1','-WaypointPass','1','-SectorMoveFire','1','-CoverHuddle','1','-PinnedFlank','1','-ArrivalThreat','1','-SectorCoverRoute','1','-ArrivalWalk','1')
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
 if(!$NoReuse){$pa+='-ReuseIdentical'}
 if($Fps -ne 60){$pa+=@('-Fps',"$Fps")}
 if($swap){$pa+='-Swap'}
 if($PlanOnly){[pscustomobject]@{Name=$name;Args=$pa};continue}
 while(@($jobs|Where-Object{!$_.HasExited}).Count -ge $Concurrency){Start-Sleep 5}
 $jobs+=Start-Process powershell.exe -ArgumentList $pa -RedirectStandardOutput "Econ-$name.txt" -RedirectStandardError "Econ-$name.err.txt" -WindowStyle Hidden -PassThru
 Start-Sleep 3
}
if($PlanOnly){return}
$jobs|ForEach-Object{$_.WaitForExit()}
"started=$($jobs.Count) skipped(existing)=$skipped"
