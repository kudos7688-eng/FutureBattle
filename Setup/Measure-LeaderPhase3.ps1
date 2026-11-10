[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputFolder)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($OutputFolder)
[IO.Directory]::CreateDirectory($out)|Out-Null
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll).Hash
$planFile=Join-Path $out 'plan.json'
if(Test-Path $planFile) {
 $old=Get-Content $planFile -Raw|ConvertFrom-Json
 if($old.dll_sha256 -ne $hash) { throw 'Cannot resume with another DLL' }
} else {
 @{created=(Get-Date -Format o);dll_sha256=$hash;expected_runs=72;warmup=10;duration=60;
 comparison='Mixed Individual (legacy individual + squad coordination rules) versus Mixed Leader';
 layouts='Existing flat large field: parallel lanes / crossing goals / three converging gates';
 seeds=@(1,2);units=@(500,1000);views=@('RTS','TPS','FPS');
 controlled_unit=41;input='move +X until simulation second 23, then stationary; no scripted player fire';
 normal_adoption=$false;reference_pc_scaling='unavailable without matched hardware measurements'} |
 ConvertTo-Json -Depth 5|Set-Content $planFile
}
$completed=0
foreach($units in @(500,1000)) { foreach($view in @('RTS','TPS','FPS')) { foreach($layout in @('Open','Cross','Gate')) { foreach($seed in @(1,2)) {
 $modes=if($seed -eq 1){@('Individual','Leader')}else{@('Leader','Individual')}
 foreach($mode in $modes) {
  if((Get-FileHash $dll).Hash -ne $hash) { throw 'DLL changed during measurement' }
  $dir=Join-Path $out "$units-$view-$layout-s$seed-$mode"
  $manifest=Join-Path $dir 'manifest.json'
  if(Test-Path $manifest) {
   $m=Get-Content $manifest -Raw|ConvertFrom-Json
   if($m.status -ne 'passed' -or $m.dll_sha256 -ne $hash) { throw "Incomplete run requires separate review: $dir" }
  } else {
   & (Join-Path $PSScriptRoot 'Test-LeaderPhase3.ps1') -OutputFolder $dir -Units $units -Decision $mode -View $view -Layout $layout -Seed $seed -Warmup 10 -Duration 60
  }
  $completed++
  "$(Get-Date -Format o) completed=$completed/72 $units $view $layout seed=$seed $mode" | Tee-Object -FilePath (Join-Path $out 'progress.txt') -Append
 }
}}}}
"Complete: $completed/72"
