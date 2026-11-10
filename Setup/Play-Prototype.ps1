[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$FastMatch, [switch]$LargeField, [ValidateSet(20,100,500)][int]$Units = 20)
$ErrorActionPreference = 'Stop'
if ($LargeField) { $Units = 500 }
if ($FastMatch -and $Units -ne 20) { throw 'Large field test and FastMatch are separate modes' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
if (!(Test-Path -LiteralPath $editor)) { throw "Editor missing: $editor" }
if (!(Test-Path -LiteralPath $project)) { throw "Project missing: $project" }
# Uses the installed editor runtime; this is not a packaged distribution build.
$extra = @()
if ($FastMatch) { $extra += '-BattleFastMatch' }
if ($Units -ne 20) { $extra += "-BattleScaleUnits=$Units" }
$map = if ($LargeField) { '/Game/Prototype/Lvl_BattleField500' } else { '/Game/Prototype/Lvl_BattlePrototype' }
& $editor $project $map -game -windowed -ResX=1280 -ResY=720 @extra
