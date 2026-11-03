[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path -LiteralPath $build)) { throw "Build tool missing: $build" }
if (!(Test-Path -LiteralPath $project)) { throw "Project missing: $project" }
& $build FutureBattleEditor Win64 Development "-Project=$([IO.Path]::GetFullPath($project))" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'Build-Editor.log')
exit $LASTEXITCODE
