[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I','J','K','L')][string]$Map='K',[ValidateSet(0,1)][int]$KitLook=1,[string]$Name='',
    [string]$Cams='0,-9000,7000,-35,90;-3000,-2000,350,-6,40;4000,3000,250,-3,200;-12000,0,1500,-12,0',[string[]]$Extra=@(),
    [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
# Look check of the asset kit (display only): starts an area match on the map, saves one picture per camera
# ("x,y,z,pitch,yaw;..." in cm and degrees; BattleKitLook.cpp StartShots) into Setup/KitShots and quits. No evaluation, no result.
# Needs the build/measurement lock (it runs the game with rendering).
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$dir=Join-Path $PSScriptRoot 'KitShots'; New-Item -ItemType Directory -Force $dir | Out-Null
if (!$Name) { $Name = "$Map-kit$KitLook" }
$prefix=(Join-Path $dir $Name).Replace([char]92,[char]47)
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1600','-ResY=900',
    '-BattleWideMixed','-BattleArea','-BattleStartRunning',"-BattleKitLook=$KitLook",('-BattleKitShot="'+$prefix+'"'),('-BattleKitShotCams="'+$Cams+'"'),('-abslog="'+$prefix+'.log"'))+$Extra
$p=Start-Process -FilePath $editor -ArgumentList $a -WindowStyle Hidden -PassThru
if (!$p.WaitForExit(240000)) { Stop-Process -Id $p.Id; throw "Shot timed out. See $prefix.log" }
Select-String -LiteralPath "$prefix.log" -Pattern 'BATTLE_KIT' | ForEach-Object { $_.Line }
Get-ChildItem $dir -Filter "$Name-*.png" | ForEach-Object { $_.FullName }
