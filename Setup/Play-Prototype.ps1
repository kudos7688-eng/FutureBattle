[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
if (!(Test-Path -LiteralPath $editor)) { throw "Editor missing: $editor" }
if (!(Test-Path -LiteralPath $project)) { throw "Project missing: $project" }
# Uses the installed editor runtime; this is not a packaged distribution build.
& $editor $project /Game/Prototype/Lvl_BattlePrototype -game -windowed -ResX=1280 -ResY=720
