# Staged one-team strategy screening (2026-11-16). Each condition is played by one team at a time against the default
# (Run-EconomyMatrix -TrialTeam 0 and 1) and compared with default-vs-default on the same seeds, which removes the side bias.
#  Stage 1: seeds 7/19/43 (12 matches per team = 24 per condition, plus 12 baseline).
#  Stage 2, only for conditions whose stage-1 mean score-lead gain is above -Threshold: seeds 61/89/101/131/157/173.
# Output folders: econ-<condition>-*-<Tag>s1t0 / s1t1 / s2t0 / s2t1 and econ-HI-*-<Tag>s1 / s2 (existing ones are reused).
# Example: .\Run-StrategyScreen.ps1 -FrozenBuild Benchmarks/BuildSnapshots/x/manifest.json -Conditions EH,ESA -Tag r20 -Fps 30 -NoRoundTripSave
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$FrozenBuild,
 [Parameter(Mandatory=$true)][string[]]$Conditions,
 [Parameter(Mandatory=$true)][string]$Tag,
 [string]$Baseline='HI',
 [double]$Threshold=0,
 [ValidateRange(10,120)][int]$Fps=60,
 [switch]$NoRoundTripSave,
 [ValidateRange(1,16)][int]$Concurrency=12,
 [string[]]$Maps=@('D','A')   # 2026-11-17: map E screens pass -Maps E
)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
$common=@{FrozenBuild=$FrozenBuild;Fps=$Fps;NoRoundTripSave=$NoRoundTripSave;Concurrency=$Concurrency;Maps=$Maps}
$stages=@(@{Name='s1';Seeds=@(7,19,43)},@{Name='s2';Seeds=@(61,89,101,131,157,173)})
function Read-Runs([string]$Condition,[string]$RunTag){
 $runs=@()
 foreach($d in Get-ChildItem $root -Directory -Filter "econ-$Condition-*-$RunTag"){
  $r=Join-Path $d.FullName 'result.json';$m=Join-Path $d.FullName 'metrics.json'
  if((Test-Path $r) -and (Test-Path $m) -and (Get-Content $r -Raw|ConvertFrom-Json).passed){$runs+=,(Get-Content $m -Raw -Encoding UTF8|ConvertFrom-Json)}
  else{Write-Host "   failed/missing: $($d.Name)"}
 }
 ,$runs
}
function Lead($runs,[int]$Team){
 if(!$runs.Count){return [double]::NaN}
 $s=0;$w=0;foreach($m in $runs){$d=$m.ally_score-$m.enemy_score;if($Team){$d=-$d};$s+=$d;$w+=[int]($m.winner -eq $Team)}
 [pscustomobject]@{Lead=$s/$runs.Count;Wins=$w;N=$runs.Count}
}
$alive=@($Conditions);$total=@{}
foreach($st in $stages){
 if(!$alive.Count){break}
 & .\Run-EconomyMatrix.ps1 @common -Conditions $Baseline -Seeds $st.Seeds -Tag "$Tag$($st.Name)"|Out-Host
 foreach($t in 0,1){& .\Run-EconomyMatrix.ps1 @common -Conditions $alive -Seeds $st.Seeds -Tag "$Tag$($st.Name)t$t" -TrialTeam $t|Out-Host}
 $base=Read-Runs $Baseline "$Tag$($st.Name)"
 $next=@()
 foreach($c in $alive){
  $gain=0;$wins=0;$bw=0;$n=0
  foreach($t in 0,1){
   $tr=Lead (Read-Runs $c "$Tag$($st.Name)t$t") $t;$b=Lead $base $t
   $gain+=$tr.Lead-$b.Lead;$wins+=$tr.Wins;$bw+=$b.Wins;$n+=$tr.N
   '{0} {1} team {2}: lead {3:N1} (baseline {4:N1}, gain {5:+0.0;-0.0}), wins {6}/{7} (baseline {8}/{9})' -f $c,$st.Name,$t,$tr.Lead,$b.Lead,($tr.Lead-$b.Lead),$tr.Wins,$tr.N,$b.Wins,$b.N
  }
  $gain/=2
  if(!$total.ContainsKey($c)){$total[$c]=@{Gain=0;Groups=0;Wins=0;Base=0;N=0}}
  $g=$total[$c];$g.Gain+=$gain*($st.Seeds.Count/3);$g.Groups+=$st.Seeds.Count/3;$g.Wins+=$wins;$g.Base+=$bw;$g.N+=$n
  '{0} {1}: mean gain {2:+0.0;-0.0}' -f $c,$st.Name,$gain
  if($gain -gt $Threshold){$next+=$c}else{"$c stopped after $($st.Name) (gain <= $Threshold)"}
 }
 $alive=$next
}
'--- summary (gain = trial-team score lead minus the default side lead on the same seeds, mean over teams and seed groups)'
foreach($c in $Conditions){$g=$total[$c];'{0}: gain {1:+0.0;-0.0} over {2} seed groups, trial wins {3}/{4} (baseline {5}/{4})' -f $c,($g.Gain/$g.Groups),$g.Groups,$g.Wins,$g.N,$g.Base}
