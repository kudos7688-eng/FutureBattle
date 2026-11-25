# Strategy-layer check (2026-11-22, Docs/design-strategy-layer.md): one wave of single-side-free matches (no swap) in parallel, each
# with the standard evaluation arguments of Run-EconomyMatrix (map, seed, cover actions + flank coordination, save round trip, 60 FPS).
# Cases are "Label=Map:Seed:Snapshot:extra args" strings; Snapshot is 'S' (-StrategyBuild) or 'B' (-BaseBuild). Existing outputs are kept.
# Example:
#  .\Run-StrategyCheck.ps1 -StrategyBuild Benchmarks/BuildSnapshots/strat-r1/manifest.json -BaseBuild <main snapshot manifest> -Tag c1 `
#   -Cases 'base=E:7:B','def=E:7:S','sl=E:7:S:-SituationLog 1'
# Outputs: Benchmarks/SightTrial-20261111/chk-<Label>-<Map><Seed>-<Tag>. Compare with Compare-MatchResults.py, summarise with
# Analyze-Situation.py.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$StrategyBuild,
 [string]$BaseBuild='',
 [Parameter(Mandatory=$true)][string[]]$Cases,
 [Parameter(Mandatory=$true)][string]$Tag,
 [ValidateRange(1,16)][int]$Concurrency=12,
 [ValidateRange(10,120)][int]$Fps=60
)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
# With powershell -File the comma list arrives as one string: split it (case strings themselves hold no comma).
$Cases=@($Cases|ForEach-Object{$_ -split ','}|Where-Object{$_})
$builds=@{S=(Resolve-Path $StrategyBuild).Path}
if($BaseBuild){$builds['B']=(Resolve-Path $BaseBuild).Path}
$jobs=@();$names=@()
foreach($c in $Cases){
 if($c -notmatch '^([A-Za-z0-9]+)=([A-J]):(\d+):([SB])(?::(.*))?$'){throw "Bad case: $c"}
 $label,$map,$seed,$b,$extra=$Matches[1],$Matches[2],$Matches[3],$Matches[4],$Matches[5]
 if(!$builds.ContainsKey($b)){throw "No build for $b in $c"}
 $name="chk-$label-$map$seed-$Tag"
 $names+=$name
 if(Test-Path "Benchmarks/SightTrial-20261111/$name"){continue}
 $pa=@('-NoProfile','-ExecutionPolicy','Bypass','-File','.\Test-WideSight.ps1','-Map',$map,'-OutputName',$name,'-Seed',$seed,
  '-AICoverProbe','0','-LeaderGuard','0','-SightRange','10000','-WeaponRange','5000','-Area','-Experiment',
  '-CoverActions','1','-FlankCoordination','1','-ObserveTactics','-BehaviorConcurrency',"$Concurrency",'-FrozenBuild',$builds[$b],'-RoundTripSave')
 if($Fps -ne 60){$pa+=@('-Fps',"$Fps")}
 if($extra){$pa+=@($extra -split '\s+'|Where-Object{$_})}
 while(@($jobs|Where-Object{!$_.HasExited}).Count -ge $Concurrency){Start-Sleep 5}
 $jobs+=Start-Process powershell.exe -ArgumentList $pa -RedirectStandardOutput "Chk-$name.txt" -RedirectStandardError "Chk-$name.err.txt" -WindowStyle Hidden -PassThru
 Start-Sleep 3
}
$jobs|ForEach-Object{$_.WaitForExit()}
foreach($n in $names){
 $r="Benchmarks/SightTrial-20261111/$n/result.json"
 if(Test-Path $r){$j=Get-Content $r -Raw|ConvertFrom-Json;"$n passed=$($j.passed)"}else{"$n (no result.json)"}
}
