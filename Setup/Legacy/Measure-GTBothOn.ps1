param([string]$Prefix='GTBothOn-20261110')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll).Hash
foreach($view in @('RTS','TPS','FPS')) {
 $order=if($view -eq 'TPS'){@('light','trace','control')}else{@('control','light','trace')}
 foreach($kind in $order) {
  if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed'}
  $folder=Join-Path $root "$view-$kind"
  & (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1') -OutputFolder $folder -Units 1000 -View $view -Stable -Sense B -ContextReuse 1 -HiddenBones 1 -CpuTrace:($kind -ne 'control') -LightTrace:($kind -eq 'light')
  if($kind -ne 'control') { & (Join-Path $PSScriptRoot 'Export-GTTrace.ps1') -Folder $folder }
  "$(Get-Date -Format o) $view $kind complete" | Tee-Object -FilePath (Join-Path $root 'progress.txt') -Append
 }
}
