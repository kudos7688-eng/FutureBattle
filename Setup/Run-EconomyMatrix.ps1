# Area economy comparison (2026-11-14): new-squad hiring x resource income, each ON/OFF. Runs Test-WideSight.ps1 for each
# against one frozen build. Existing output folders are skipped, so an interrupted matrix can be resumed by re-running
# the same command. Summarise with Setup/Analyze-EconomyMatrix.ps1.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$FrozenBuild,   # e.g. Benchmarks/BuildSnapshots/claude-supply-r1/manifest.json
 [string[]]$Conditions=@('HI','H','I','none'),
 [int[]]$Seeds=@(7,19,43),
 [string[]]$Maps=@('D','A'),
 [ValidateRange(1,16)][int]$Concurrency=8,
 [string]$Tag='r1'
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
 # Suppression trial: P = defaults (spread x2.5 at full, pin at 0.5); PS = spread only (no pinning); PP = pinning only; PH = pin at 0.75.
 'P'=@('-Suppression','1')
 'PS'=@('-Suppression','1','-SuppressionPin','2')
 'PP'=@('-Suppression','1','-SuppressionSpread','0')
 'PH'=@('-Suppression','1','-SuppressionPin','0.75')
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
  '-AICoverProbe','0','-LeaderGuard','0','-SightRange','10000','-WeaponRange','5000','-Area','-Experiment','-RoundTripSave',
  '-CoverActions','1','-FlankCoordination','1','-ObserveTactics','-BehaviorConcurrency',"$Concurrency",'-FrozenBuild',$frozen)+$supply[$c]
 if($swap){$pa+='-Swap'}
 while(@($jobs|Where-Object{!$_.HasExited}).Count -ge $Concurrency){Start-Sleep 5}
 $jobs+=Start-Process powershell.exe -ArgumentList $pa -RedirectStandardOutput "Econ-$name.txt" -RedirectStandardError "Econ-$name.err.txt" -WindowStyle Hidden -PassThru
 Start-Sleep 3
}
$jobs|ForEach-Object{$_.WaitForExit()}
"started=$($jobs.Count) skipped(existing)=$skipped"
