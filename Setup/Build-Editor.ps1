[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path -LiteralPath $build)) { throw "Build tool missing: $build" }
if (!(Test-Path -LiteralPath $project)) { throw "Project missing: $project" }
# UE 5.8 can reuse a stale source list even with -gather/-NoUBTMakefiles.
# A changed project timestamp explicitly invalidates its makefile (no project content changes).
$projectFolder = Split-Path ([IO.Path]::GetFullPath($project))
$inventoryFile = Join-Path $projectFolder 'Intermediate\CodexSourceInventory.txt'
$sourceInventory = (Get-ChildItem -LiteralPath (Join-Path $projectFolder 'Source') -Recurse -File | Where-Object { $_.Extension -in '.cpp', '.h', '.cs' } | ForEach-Object { $_.FullName } | Sort-Object) -join "`n"
$previousInventory = if (Test-Path -LiteralPath $inventoryFile) { [IO.File]::ReadAllText($inventoryFile) } else { '' }
if ($sourceInventory -ne $previousInventory) { (Get-Item -LiteralPath $project).LastWriteTimeUtc = [DateTime]::UtcNow }
& $build FutureBattleEditor Win64 Development "-Project=$([IO.Path]::GetFullPath($project))" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'Build-Editor.log')
$buildExit = $LASTEXITCODE
if ($buildExit -eq 0) { [IO.File]::WriteAllText($inventoryFile, $sourceInventory) }
exit $buildExit
