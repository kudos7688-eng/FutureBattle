[CmdletBinding()]
param([string]$Prefix='PerceptionAudit-final-20261110')
$ErrorActionPreference='Stop'
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll).Hash
foreach($audit in @($false,$true)) {
 $kind=if($audit){'sense'}else{'perf'}
 $root=Join-Path $PSScriptRoot "Benchmarks/$Prefix-$kind"
 [IO.Directory]::CreateDirectory($root)|Out-Null
 $plan=@{expected_runs=12;status='running';dll_sha256=$hash;sense_audit=$audit;performance_measurement=!$audit;warmup=10;duration=60;units=@(500,1000);views=@('RTS','TPS','FPS');layout='Open';seed=1;stable=$true;comparison='A individual due observers; B actual normal four-slice batch. Both grid/far OFF';old_phase3_combined=$false}
 $plan|ConvertTo-Json -Depth 6|Set-Content (Join-Path $root 'plan.json')
 $completed=0
 foreach($n in @(500,1000)) { foreach($v in @('RTS','TPS','FPS')) {
  $order=if($v -eq 'TPS'){@('B','A')}else{@('A','B')}
  foreach($sense in $order) {
   if((Get-FileHash $dll).Hash -ne $hash) { throw 'DLL changed' }
   $folder=Join-Path $root "$n-$v-$sense"
   if(Test-Path (Join-Path $folder 'manifest.json')) { throw "Use a fresh prefix: $folder" }
   & (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1') -OutputFolder $folder -Units $n -View $v -Stable -Sense $sense -SenseAudit:$audit
   $completed++
   "$(Get-Date -Format o) $kind $completed/12 $n $v $sense" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
  }
 }}
 $plan.status='complete';$plan|ConvertTo-Json -Depth 6|Set-Content (Join-Path $root 'plan.json')
}
