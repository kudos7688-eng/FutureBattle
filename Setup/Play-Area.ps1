[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G')][string]$Map='A')
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' $project "/Game/Prototype/Lvl_BattleWide_$Map" -game -windowed -ResX=1920 -ResY=1080 -ForceRes -BattleWideMixed -BattleArea -BattleCoverActions=1 -BattleFlankCoordination=1
