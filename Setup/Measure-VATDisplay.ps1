param([string]$Prefix='DisplayLOD-20261111-r2',[ValidateSet('Pose','Quality','Functional','Replay','Perf','Trace')][string]$Stage)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
[IO.Directory]::CreateDirectory($root)|Out-Null
$hash=(Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')).Hash
function Run([string]$name,[hashtable]$options) {
 if((Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')).Hash -ne $hash) {throw 'DLL changed'}
 & (Join-Path $PSScriptRoot 'Test-VATDisplay.ps1') -OutputFolder (Join-Path $root $name) -ContextReuse 1 -HiddenBones 1 -PerceptionGrid 1 -Sense B -AnimTasks 0 @options
 "$(Get-Date -Format o) $name passed" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
}
if($Stage -eq 'Pose') {
 foreach($view in @('RTS','TPS','FPS')) {Run "pose-$view" @{Units=40;View=$view;Stable=$true;DisplayLOD=1;VATAudit=$true;Warmup=0;Duration=15}}
}
if($Stage -eq 'Functional') {
 foreach($mode in @(0,1)) {Run "functional-$mode" @{Units=40;View='RTS';Stable=$true;DisplayLOD=$mode;Functional=$true;NullRHI=$true;Warmup=0;Duration=1}}
}
if($Stage -eq 'Replay') {
 foreach($view in @('RTS','TPS','FPS')) {
  foreach($mode in @(0,1)) {Run "replay1000-$view-$mode" @{Units=1000;View=$view;DisplayLOD=$mode;ContextReplay=$true;Capture=$true;Warmup=0;Duration=45}}
  & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' (Join-Path $PSScriptRoot 'Check-SightContext.py') (Join-Path $root "replay1000-$view-0") (Join-Path $root "replay1000-$view-1")
  if($LASTEXITCODE -ne 0) {throw "Replay mismatch $view"}
 }
}
if($Stage -eq 'Perf') {
 foreach($rep in @(1,2)) {foreach($view in @('RTS','TPS','FPS')) {
  $order=if(($rep -eq 1) -xor ($view -eq 'TPS')){@(0,1)}else{@(1,0)}
  foreach($mode in $order) {Run "perf-$view-r$rep-$mode" @{Units=1000;View=$view;Stable=$true;DisplayLOD=$mode}}
 }}
}

if($Stage -eq 'Trace') {
 foreach($mode in @(0,1)) {
  $name="trace-RTS-$mode"
  Run $name @{Units=1000;View='RTS';Stable=$true;DisplayLOD=$mode;CpuTrace=$true}
  & (Join-Path $PSScriptRoot 'Export-VATTrace.ps1') -Folder (Join-Path $root $name) -FullEvents
 }
}


if($Stage -eq 'Quality') { foreach($view in @('RTS','TPS','FPS')) {Run "quality-$view" @{Units=40;View=$view;Stable=$true;DisplayLOD=1;VATAudit=$true;VATQuality=$true;Warmup=0;Duration=15}} }

