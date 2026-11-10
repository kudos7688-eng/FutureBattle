[CmdletBinding()]
param([ValidateSet(20,40,60,80,100)][int]$Units=40,[int]$NoControlGroup=-1,[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
if(!(Test-Path $editor)) { throw "Missing editor: $editor" }
# Explicit isolated experimental mode. The ordinary launcher remains unchanged.
& $editor $project '/Game/Prototype/Lvl_BattlePrototype' -game -windowed -ResX=1280 -ResY=720 -ForceRes -BattleLeaderCommand=1 "-BattleLeaderUnits=$Units" "-BattleLeaderNoControl=$NoControlGroup"
