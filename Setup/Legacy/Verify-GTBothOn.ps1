param([string]$Prefix='GTBothOn-20261110')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
$manifests=@(Get-ChildItem $root -Filter manifest.json -Recurse | ForEach-Object { Get-Content $_.FullName -Raw | ConvertFrom-Json })
if($manifests.Count -ne 9 -or @($manifests | Where-Object status -ne 'passed').Count) { throw 'Expected nine passed runs' }
$base=Get-Content (Join-Path $root 'RTS-trace/manifest.json') -Raw | ConvertFrom-Json
$sourceChanged=@($base.source_hashes | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Hash })
$configChanged=@($base.config_hashes | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Hash })
$protected=Get-Content (Join-Path $PSScriptRoot 'Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json') -Raw | ConvertFrom-Json
$protectedChanged=@($protected | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Hash })
$dll=(Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')).Hash
$map=(Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')).Hash
$initial=Get-Content (Join-Path $root 'plan.json') -Raw | ConvertFrom-Json
if($dll -ne $initial.dll -or $map -ne $initial.map){throw 'Initial DLL/map changed'}
foreach($name in @('before-source.json','before-config.json')) {
 foreach($x in (Get-Content (Join-Path $root $name) -Raw | ConvertFrom-Json)) {
  if((Get-FileHash -LiteralPath $x.Path).Hash -ne $x.Hash){throw "Initial file changed: $($x.Path)"}
 }
}
foreach($m in $manifests) {foreach($x in ($m.source_hashes+$m.config_hashes)) {if((Get-FileHash -LiteralPath $x.Path).Hash -ne $x.Hash){throw 'Run file changed'}}}
$off=Get-Content (Join-Path $PSScriptRoot 'Benchmarks/HiddenBones-20261110-off/manifest.json') -Raw | ConvertFrom-Json
if($off.status -ne 'complete' -or $off.dll_sha256 -ne $dll -or $off.matches -ne 40 -or $off.smoke_pass -ne 210){throw 'Prior regression evidence mismatch'}
$bad=@($manifests | Where-Object { $_.dll_sha256 -ne $dll -or $_.nullrhi -or $_.sense -ne 'B' -or $_.sense_audit -or $_.units -ne 1000 -or !$_.stable -or $_.layout -ne 'Open' -or $_.seed -ne 1 -or !$_.machine })
$gpuVerified=@(Get-ChildItem $root -Filter run.log -Recurse | ForEach-Object {
 $log=Get-Content $_.FullName -Raw
 if($log -notmatch 'Chosen D3D12 Adapter Id = (\d+)') { throw 'Missing selected adapter' }
 $id=$Matches[1]
 if($log -notmatch ('Found D3D12 adapter '+$id+': NVIDIA GeForce RTX 5070 Ti')) { throw 'Unexpected selected GPU' }
 $_.FullName
})
if(@($manifests | Where-Object { $_.context_reuse -ne 1 -or $_.hidden_bones -ne 1 -or $_.context_replay -or $_.bone_audit }).Count) { throw 'Both-on flags or instrumentation mismatch' }
if($gpuVerified.Count -ne 9) { throw 'Expected nine GPU validations' }
$summaries=@(Get-ChildItem $root -Filter analysis.json -Recurse | ForEach-Object { Get-Content $_.FullName -Raw | ConvertFrom-Json })
$badSummaries=@($summaries | Where-Object { !$_.validation.all_alive -or !$_.validation.all_rvo -or !$_.validation.gpu_present -or $_.validation.views.Count -ne 1 -or $_.validation.views[0] -ne ([IO.Path]::GetFileName($_.folder) -split '-')[0] })
if($sourceChanged.Count -or $configChanged.Count -or $protectedChanged.Count -or $map -ne $base.map_hash -or $bad.Count -or $badSummaries.Count -or $summaries.Count -ne 9) {
 @{source=$sourceChanged;config=$configChanged;protected=$protectedChanged;map_equal=($map -eq $base.map_hash);bad_runs=@($bad|Select-Object view,status,adapter);bad_summaries=@($badSummaries|Select-Object folder,validation);summary_count=$summaries.Count}|ConvertTo-Json -Depth 8|Write-Output
 throw 'Verification failed'
}
$r=@{checked=(Get-Date -Format o);passed_runs=9;dll=$dll;source_count=$base.source_hashes.Count;source_changed=$sourceChanged;config_changed=$configChanged;map_unchanged=$true;protected_count=$protected.Count;protected_changed=$protectedChanged;all_alive_and_rvo_1000=$true;actual_views_and_gpu_verified=$true;additional_optimization_implemented=$false;context_reuse=1;hidden_bones=1;regression_rerun=$false;existing_same_dll_regression=@{integrated_pass=210;off_matches=40;phase3_functional_pass=25};scripts=@(Get-FileHash (Join-Path $PSScriptRoot 'Test-PerceptionAudit.ps1'),(Join-Path $PSScriptRoot 'Export-GTTrace.ps1'),(Join-Path $PSScriptRoot 'Analyze-GTTrace.py'),(Join-Path $PSScriptRoot 'Report-GTBothOn.py') | Select-Object Path,Hash)}
$r|ConvertTo-Json -Depth 9|Set-Content (Join-Path $root 'verification.json')
$plan=Get-Content (Join-Path $root 'plan.json') -Raw | ConvertFrom-Json
$plan.status='complete';$plan|ConvertTo-Json -Depth 9|Set-Content (Join-Path $root 'plan.json')
$r|ConvertTo-Json -Depth 3
