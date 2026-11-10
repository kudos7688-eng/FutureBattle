param([string]$Prefix='GTBreakdown-20261110')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
# First RTS trace is collected separately to validate the exporter before this batch.
foreach($item in @(@('RTS',$false),@('TPS',$false),@('TPS',$true),@('FPS',$true),@('FPS',$false))) {
 $view=$item[0]; $trace=[bool]$item[1]; $kind=if($trace){'trace'}else{'control'}
 $folder=Join-Path $root "$view-$kind"
 & (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1') -OutputFolder $folder -Units 1000 -View $view -Stable -Sense B -CpuTrace:$trace
 if($trace) { & (Join-Path $PSScriptRoot 'Export-GTTrace.ps1') -Folder $folder }
 "$(Get-Date -Format o) $view $kind complete" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
}
