param([string]$Prefix='GTBreakdown-20261110')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
foreach($view in @('RTS','TPS','FPS')) {
 $folder=Join-Path $root "$view-light"
 & (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1') -OutputFolder $folder -Units 1000 -View $view -Stable -Sense B -CpuTrace -LightTrace
 & (Join-Path $PSScriptRoot 'Export-GTTrace.ps1') -Folder $folder
 "$(Get-Date -Format o) $view light complete" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
}
