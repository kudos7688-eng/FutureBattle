# Area home supply cycle comparison (2026-11-13). Runs Test-WideSight.ps1 for each condition x seed x map x side swap
# against one frozen build. Existing output folders are skipped, so an interrupted matrix can be resumed by re-running
# the same command. Analyse with Setup/Analyze-SupplyMatrix.ps1.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$FrozenBuild,   # e.g. Benchmarks/BuildSnapshots/claude-supply-r1/manifest.json
 [string[]]$Conditions=@('base','A','B','C','D','all'),
 [int[]]$Seeds=@(7,19,43),
 [string[]]$Maps=@('D','A'),
 [ValidateRange(1,4)][int]$Concurrency=4,
 [string]$Tag='r1'
)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$frozen=(Resolve-Path $FrozenBuild).Path
# Supply trial arguments per condition. 'base' passes none (Config defaults = behaviour before 2026-11-13).
$supply=@{
 'base'=@()
 'A'=@('-SupplyReturnLiving','6')
 'B'=@('-SupplyHeal','10','-SupplyResumeHealth','0.9')
 'C'=@('-SupplyResumeLiving','9')
 'D'=@('-SupplyAwayRatio','0.34')
 'all'=@('-SupplyReturnLiving','6','-SupplyHeal','10','-SupplyResumeHealth','0.9','-SupplyResumeLiving','9','-SupplyAwayRatio','0.34')
}
$runs=@()
foreach($c in $Conditions){ if(!$supply.ContainsKey($c)){throw "Unknown condition $c"}
 foreach($seed in $Seeds){ foreach($map in $Maps){ foreach($swap in $false,$true){ $runs+=,@($c,$seed,$map,$swap) } } } }
$jobs=@(); $skipped=0
foreach($r in $runs){
 $c,$seed,$map,$swap=$r
 $name="supply-$c-$map-s$seed"+($(if($swap){'-swap'}else{''}))+"-$Tag"
 if(Test-Path "Benchmarks/SightTrial-20261111/$name"){ $skipped++; continue }
 $pa=@('-NoProfile','-ExecutionPolicy','Bypass','-File','.\Test-WideSight.ps1','-Map',$map,'-OutputName',$name,'-Seed',"$seed",
  '-AICoverProbe','0','-LeaderGuard','0','-SightRange','10000','-WeaponRange','5000','-Area','-Experiment','-RoundTripSave',
  '-CoverActions','1','-FlankCoordination','1','-ObserveTactics','-BehaviorConcurrency',"$Concurrency",'-FrozenBuild',$frozen)+$supply[$c]
 if($swap){$pa+='-Swap'}
 while(@($jobs|Where-Object{!$_.HasExited}).Count -ge $Concurrency){Start-Sleep 5}
 $jobs+=Start-Process powershell.exe -ArgumentList $pa -RedirectStandardOutput "Supply-$name.txt" -RedirectStandardError "Supply-$name.err.txt" -WindowStyle Hidden -PassThru
 Start-Sleep 3
}
$jobs|ForEach-Object{$_.WaitForExit()}
"started=$($jobs.Count) skipped(existing)=$skipped"
