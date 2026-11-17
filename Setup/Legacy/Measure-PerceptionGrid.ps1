param([string]$Prefix='PerceptionGrid-20261110',[ValidateSet('Functional','Replay','Audit','Perf')][string]$Stage)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
[IO.Directory]::CreateDirectory($root)|Out-Null
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll).Hash
function Run([string]$name,[hashtable]$options) {
 if((Get-FileHash $dll).Hash -ne $hash) {throw 'DLL changed'}
 & (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1') -OutputFolder (Join-Path $root $name) -ContextReuse 1 -HiddenBones 1 -Sense B @options
 "$(Get-Date -Format o) $name passed" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
}
function Check([string[]]$names) {
 $paths=@($names | ForEach-Object {Join-Path $root $_})
 & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' (Join-Path $PSScriptRoot 'Check-PerceptionGrid.py') @paths
 if($LASTEXITCODE -ne 0){throw 'Grid equivalence failed'}
}
if($Stage -eq 'Functional') {
 foreach($mode in @(0,1)) {Run "functional-$mode" @{Units=40;View='RTS';Stable=$true;PerceptionGrid=$mode;Functional=$true;GridFixture=$true;NullRHI=$true;Warmup=0;Duration=1}}
}
if($Stage -eq 'Replay') {
 foreach($view in @('RTS','TPS','FPS')) {
  foreach($mode in @(0,1)) {Run "replay1000-$view-$mode" @{Units=1000;View=$view;PerceptionGrid=$mode;ContextReplay=$true;NullRHI=$true;Warmup=0;Duration=45}}
  Check @("replay1000-$view-0","replay1000-$view-1")
 }
}
if($Stage -eq 'Audit') {
 foreach($view in @('RTS','TPS','FPS')) {Run "audit-$view" @{Units=1000;View=$view;Stable=$true;PerceptionGrid=1;GridAudit=$true};Check @("audit-$view")}
}
if($Stage -eq 'Perf') {
 foreach($rep in @(1,2)) {foreach($view in @('RTS','TPS','FPS')) {
  $order=if(($rep -eq 1) -xor ($view -eq 'TPS')){@(0,1)}else{@(1,0)}
  foreach($mode in $order) {Run "perf-$view-r$rep-$mode" @{Units=1000;View=$view;Stable=$true;PerceptionGrid=$mode}}
 }}
}
