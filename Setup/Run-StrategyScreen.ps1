# One-team strategy screening (2026-11-16; sequential stopping and 60 FPS confirmation since 2026-11-18).
# Each condition is played by one team at a time against the default (Run-EconomyMatrix -TrialTeam 0 and 1) and compared
# with default-vs-default on the same seeds, which removes the side bias. Per seed, gain = mean over the two trial teams of
# (trial-team score lead - that side's lead in the default-vs-default matches), each over maps x swap.
#  Screening (-Fps, default 30): seeds 7, 19, 43, 61, 89, 101, 131, 157, 173 are played a few at a time (as many as fill
#   -Concurrency). After each batch, with n seeds, mean m and se = max(sample sd, -SeedSd) / sqrt(n):
#    m + 2 se < Threshold -> stop (clearly worse); n >= 3 and m - 2 se > Threshold -> pass early (clearly better);
#    n = 3 and m <= Threshold -> stop (the former stage-1 rule); all 9 seeds -> pass if m > Threshold.
#  Confirmation (-ConfirmFps, default 60; skipped with -NoConfirm or when equal to -Fps): the conditions that passed are
#   screened again by the same rule at that rate. A condition counts as adopted-worthy only if it passes both.
# Output folders: econ-<condition>-<map>-s<seed>[-swap]-<RunTag><stage>t<team> and econ-<Baseline>-...-<RunTag><stage>,
#  with RunTag = -Tag for screening and <Tag>f<ConfirmFps> for confirmation, stage s1 = seeds 7/19/43, s2 = the rest.
#  Existing folders are reused (so earlier screens with the same tag still count), identical matches too (see -NoReuse).
# -DryRun evaluates the existing folders only, without starting matches.
# Example: .\Run-StrategyScreen.ps1 -FrozenBuild Benchmarks/BuildSnapshots/x/manifest.json -Conditions EH,ESA -Tag r20
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$FrozenBuild,
 [Parameter(Mandatory=$true)][string[]]$Conditions,
 [Parameter(Mandatory=$true)][string]$Tag,
 [string]$Baseline='HI',
 [double]$Threshold=0,
 [ValidateRange(10,120)][int]$Fps=30,          # screening rate (30 = about 1.7x faster than 60, results differ slightly)
 [ValidateRange(10,120)][int]$ConfirmFps=60,   # confirmation rate for the conditions that pass screening
 [switch]$NoConfirm,
 [double]$SeedSd=60,   # floor for the per-seed gain spread (map E screens e1/e2 of 2026-11-17: sample sd 8-54 per seed)
 [switch]$NoRoundTripSave,
 [switch]$NoReuse,   # 2026-11-18: identical matches (e.g. the baseline of an earlier screen on the same build) are reused unless set
 [ValidateRange(1,16)][int]$Concurrency=12,
 [string[]]$Maps=@('D','A'),   # 2026-11-17: map E screens pass -Maps E
 [switch]$DryRun
)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
$allSeeds=@(7,19,43,61,89,101,131,157,173)
function StageOf([int]$Seed){if(@(7,19,43) -contains $Seed){'s1'}else{'s2'}}
function Read-Runs([string]$Condition,[int]$Seed,[string]$RunTag){
 $runs=@()
 foreach($d in @(Get-ChildItem $root -Directory -Filter "econ-$Condition-*-s$Seed-$RunTag")+@(Get-ChildItem $root -Directory -Filter "econ-$Condition-*-s$Seed-swap-$RunTag")){
  if(@($Maps|Where-Object{$d.Name -like "econ-$Condition-$_-*"}).Count -eq 0){continue}
  $r=Join-Path $d.FullName 'result.json';$m=Join-Path $d.FullName 'metrics.json'
  if((Test-Path $r) -and (Test-Path $m) -and (Get-Content $r -Raw|ConvertFrom-Json).passed){$runs+=,(Get-Content $m -Raw -Encoding UTF8|ConvertFrom-Json)}
  else{Write-Host "   failed/missing: $($d.Name)"}
 }
 ,$runs
}
function Lead($runs,[int]$Team){
 if(!$runs.Count){return $null}
 $s=0;$w=0;foreach($m in $runs){$d=$m.ally_score-$m.enemy_score;if($Team){$d=-$d};$s+=$d;$w+=[int]($m.winner -eq $Team)}
 [pscustomobject]@{Lead=$s/$runs.Count;Wins=$w;N=$runs.Count}
}
function SeedGain([string]$Condition,[int]$Seed,[string]$RunTag){
 $st=StageOf $Seed;$base=Read-Runs $Baseline $Seed "$RunTag$st";$g=0;$w=0;$bw=0;$n=0
 foreach($t in 0,1){
  $tr=Lead (Read-Runs $Condition $Seed "$RunTag${st}t$t") $t;$b=Lead $base $t
  if(!$tr -or !$b){return $null}
  $g+=($tr.Lead-$b.Lead)/2;$w+=$tr.Wins;$bw+=$b.Wins;$n+=$tr.N
 }
 [pscustomobject]@{Gain=$g;Wins=$w;Base=$bw;N=$n}
}
function Invoke-Pool($specs){
 $jobs=@()
 foreach($s in $specs){
  if(Test-Path (Join-Path $root $s.Name)){continue}
  while(@($jobs|Where-Object{!$_.HasExited}).Count -ge $Concurrency){Start-Sleep 5}
  $jobs+=Start-Process powershell.exe -ArgumentList $s.Args -RedirectStandardOutput "Econ-$($s.Name).txt" -RedirectStandardError "Econ-$($s.Name).err.txt" -WindowStyle Hidden -PassThru
  Start-Sleep 3
 }
 $jobs|ForEach-Object{$_.WaitForExit()}
}
function Invoke-Screen([string[]]$Candidates,[int]$PhaseFps,[string]$RunTag){
 $common=@{FrozenBuild=$FrozenBuild;Fps=$PhaseFps;NoRoundTripSave=$NoRoundTripSave;Concurrency=$Concurrency;Maps=$Maps;NoReuse=$NoReuse;PlanOnly=$true}
 $gains=@{};foreach($c in $Candidates){$gains[$c]=@()}
 $alive=@($Candidates);$passed=@();$i=0
 while($alive.Count -and $i -lt $allSeeds.Count){
  $perSeed=$Maps.Count*2*(1+2*$alive.Count)
  $end=[Math]::Min($allSeeds.Count,$i+[Math]::Max(1,[Math]::Floor($Concurrency/$perSeed)))
  if($i -lt 3){$end=[Math]::Min($end,3)}   # batches do not cross the 3-seed check
  $batch=@($allSeeds[$i..($end-1)]);$i=$end
  if(!$DryRun){
   $specs=@()
   foreach($seed in $batch){
    $st=StageOf $seed
    $specs+=@(& .\Run-EconomyMatrix.ps1 @common -Conditions $Baseline -Seeds $seed -Tag "$RunTag$st")
    foreach($t in 0,1){$specs+=@(& .\Run-EconomyMatrix.ps1 @common -Conditions $alive -Seeds $seed -Tag "$RunTag${st}t$t" -TrialTeam $t)}
   }
   "[$RunTag] seeds $($batch -join ','): $($specs.Count) matches for $($alive -join ',')"|Out-Host
   Invoke-Pool $specs
  }
  $next=@()
  foreach($c in $alive){
   foreach($seed in $batch){$g=SeedGain $c $seed $RunTag;if($g){$gains[$c]+=,$g}else{Write-Host "   $c seed ${seed}: incomplete, not counted"}}
   $v=@($gains[$c]|ForEach-Object{$_.Gain});$n=$v.Count
   if(!$n){$next+=$c;continue}
   $m=($v|Measure-Object -Average).Average
   $sd=if($n -ge 2){[Math]::Sqrt((($v|ForEach-Object{($_-$m)*($_-$m)})|Measure-Object -Sum).Sum/($n-1))}else{0}
   $se=[Math]::Max($sd,$SeedSd)/[Math]::Sqrt($n)
   $line='{0} [{1}] {2} seeds: mean gain {3:+0.0;-0.0} (sd {4:N1}, se {5:N1})' -f $c,$RunTag,$n,$m,$sd,$se
   if($m+2*$se -lt $Threshold){"$line -> stop (clearly worse)"}
   elseif($n -ge 3 -and $m-2*$se -gt $Threshold){"$line -> pass (clearly better)";$passed+=$c}
   elseif($n -eq 3 -and $i -eq 3 -and $m -le $Threshold){"$line -> stop (3-seed rule)"}
   elseif($i -ge $allSeeds.Count){if($m -gt $Threshold){"$line -> pass";$passed+=$c}else{"$line -> stop"}}
   else{"$line -> continue";$next+=$c}
  }
  $alive=$next
 }
 '--- [{0}] summary (gain = trial-team score lead minus the default side lead on the same seeds, mean over teams and seeds)' -f $RunTag
 foreach($c in $Candidates){
  $v=@($gains[$c]);if(!$v.Count){"${c}: no complete seeds";continue}
  '{0}: gain {1:+0.0;-0.0} over {2} seeds, trial wins {3}/{4} (baseline {5}/{4}), {6}' -f $c,(($v|ForEach-Object{$_.Gain}|Measure-Object -Average).Average),$v.Count,(($v|ForEach-Object{$_.Wins}|Measure-Object -Sum).Sum),(($v|ForEach-Object{$_.N}|Measure-Object -Sum).Sum),(($v|ForEach-Object{$_.Base}|Measure-Object -Sum).Sum),$(if($passed -contains $c){'passed'}else{'stopped'})
 }
 $script:LastPassed=$passed
}
Invoke-Screen $Conditions $Fps $Tag
$screened=@($script:LastPassed)
if($NoConfirm -or $ConfirmFps -eq $Fps -or !$screened.Count){"screening passed: $($screened -join ',')$(if(!$NoConfirm -and $ConfirmFps -ne $Fps){' (nothing to confirm)'}else{' (not confirmed)'})";return}
Invoke-Screen $screened $ConfirmFps "${Tag}f$ConfirmFps"
"confirmed at $ConfirmFps FPS: $(@($script:LastPassed) -join ',')"
