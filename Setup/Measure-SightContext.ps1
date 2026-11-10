param([string]$Prefix='SightContext-20261110',[ValidateSet('Functional','Replay','ReplayLarge','Audit','Perf')][string]$Stage,[ValidateSet('Open','Cross','Gate')][string[]]$ReplayLayouts=@('Open','Cross','Gate'))
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
[IO.Directory]::CreateDirectory($root)|Out-Null
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll).Hash
function Run([string]$name,[hashtable]$options) {
 if((Get-FileHash $dll).Hash -ne $hash) {throw 'DLL changed'}
 & (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1') -OutputFolder (Join-Path $root $name) @options
 "$(Get-Date -Format o) $name passed" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
}
if($Stage -eq 'Functional') {
 foreach($reuse in @(0,1)) {Run "functional-$reuse" @{Units=40;View='RTS';Stable=$true;Sense='B';Warmup=0;Duration=1;NullRHI=$true;Functional=$true;ContextReuse=$reuse}}
}
if($Stage -eq 'Replay') {
 foreach($layout in $ReplayLayouts) {
  foreach($reuse in @(0,1)) {
   Run "replay-$layout-$reuse" @{Units=40;View='RTS';Layout=$layout;Sense='B';Warmup=10;Duration=60;ContextReuse=$reuse;ContextAudit=$true;ContextReplay=$true;NullRHI=$true}
  }
  & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' (Join-Path $PSScriptRoot 'Check-SightContext.py') (Join-Path $root "replay-$layout-0") (Join-Path $root "replay-$layout-1")
  if($LASTEXITCODE -ne 0) {throw "Replay mismatch: $layout"}
 }
}
if($Stage -eq 'Audit') {
 foreach($view in @('RTS','TPS','FPS')) {
  Run "audit-$view" @{Units=1000;View=$view;Stable=$true;Sense='B';ContextReuse=1;ContextAudit=$true}
  & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' (Join-Path $PSScriptRoot 'Check-SightContext.py') (Join-Path $root "audit-$view")
  if($LASTEXITCODE -ne 0) {throw "Sense mismatch: $view"}
 }
}
if($Stage -eq 'ReplayLarge') {
 foreach($reuse in @(0,1)) {Run "replay1000-$reuse" @{Units=1000;View='RTS';Sense='B';Warmup=0;Duration=45;ContextReuse=$reuse;ContextReplay=$true;NullRHI=$true}}
 & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' (Join-Path $PSScriptRoot 'Check-SightContext.py') (Join-Path $root 'replay1000-0') (Join-Path $root 'replay1000-1')
 if($LASTEXITCODE -ne 0) {throw '1000-unit replay mismatch'}
}
if($Stage -eq 'Perf') {
 foreach($rep in @(1,2)) { foreach($view in @('RTS','TPS','FPS')) {
  $order=if(($rep -eq 1) -xor ($view -eq 'TPS')){@(0,1)}else{@(1,0)}
  foreach($reuse in $order) {Run "perf-$view-r$rep-$reuse" @{Units=1000;View=$view;Stable=$true;Sense='B';ContextReuse=$reuse}}
 }}
}
