[CmdletBinding()]
# Quit menu display check (2026-11-25): a rendered area match with the Play-Area conditions; once the combat phase runs, the top bar's
# toolbar is shown (shot-button.png), clicks on its grid and speed buttons toggle them, a left click on its quit button on it opens the confirmation (shot-open.png), a click on its back button closes
# it, Esc opens it again and a click on its quit button ends the game. Passes when the BATTLE_QUIT_SHOT lines report each step, the
# log has QUIT_MENU quit, the game exits by itself and both pictures are there.
param([string]$Name='quitmenu-r1',[ValidateSet('A','E','F','G','H','I','J','K')][string]$Map='I',[int]$ResX=1920,[int]$ResY=1080,[string]$Extra='')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/QuitMenuShot-20261125/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$log=Join-Path $out 'run.log'
$shot=Join-Path $out 'shot'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ForceRes',"-ResX=$ResX","-ResY=$ResY",
 '-BattleWideMixed','-BattleArea','-BattleCoverActions=1','-BattleFlankCoordination=1','-BattleAreaPointSupply=1','-BattleFollowerAnimSkip=1',
 ('-BattleQuitMenuShot="'+$shot+'"'),('-abslog="'+$log+'"'))
if($Extra){$a+=@($Extra -split ' '|Where-Object{$_})}
if($Map -eq 'H' -or $Map -eq 'I'){$a+=@('-BattleLeaderGuard=1','-BattleSectorMove=1','-BattleGroupFireSupport=1')}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(300000)){Stop-Process -Id $p.Id;throw 'Quit menu shot timeout'}
$t=Get-Content $log -Raw
$lines=@($t -split "`n"|Where-Object{$_ -match 'BATTLE_QUIT_SHOT'})
$button=$lines|Where-Object{$_ -match 'stage=button' -and $_ -match 'fits=1'}
$tools=$lines|Where-Object{$_ -match 'stage=tools' -and $_ -match 'grid=1' -and $_ -match 'speed=1' -and $_ -match 'restored=1'}
$open=$lines|Where-Object{$_ -match 'stage=open' -and $_ -match 'taken=1' -and $_ -match 'open=1'}
$back=$lines|Where-Object{$_ -match 'stage=back' -and $_ -match 'closed=1' -and $_ -match 'esc_reopened=1'}
$quit=$t -match 'QUIT_MENU quit'
$ok=$button -and $tools -and $open -and $back -and $quit -and (Test-Path "$shot-button.png") -and (Test-Path "$shot-open.png") -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=[bool]$ok;exitCode=$p.ExitCode;map=$Map;resolution="${ResX}x${ResY}"}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$lines
if(!$ok){throw "Quit menu shot failed: $log"}
Write-Output "Quit menu shot passed: $out"
