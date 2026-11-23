[CmdletBinding()]
# Purchase panel display check (2026-11-23): a rendered area match with the Play-Area conditions; once the combat phase runs the purchase
# panel (B) is opened (screenshot shot-open.png), the first row that can be bought is bought through the panel (shot-bought.png), the pad's
# D-pad down moves the highlight and its B closes the panel, then quit. Passes when the three BATTLE_BUY_PANEL_SHOT lines report the panel
# open with every row hit and inside the view, a purchase, the pad move and close, and both pictures are there. With reservations (default
# on since 2026-11-23 evening) two more lines and pictures: a tank reserved through the panel with too few resources (shot-reserved.png),
# then bought by the queue once resources are back (shot-reserve-bought.png, with its notice).
param([string]$Name='buypanel-r1',[ValidateSet('A','E','F','G','H','I','J','K')][string]$Map='I',[int]$ResX=1920,[int]$ResY=1080,[string]$Extra='')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/BuyPanelShot-20261123/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$log=Join-Path $out 'run.log'
$shot=Join-Path $out 'shot'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ForceRes',"-ResX=$ResX","-ResY=$ResY",
 '-BattleWideMixed','-BattleArea','-BattleCoverActions=1','-BattleFlankCoordination=1','-BattleAreaPointSupply=1','-BattleFollowerAnimSkip=1',
 ('-BattleBuyPanelShot="'+$shot+'"'),('-abslog="'+$log+'"'))
# -Extra: more launch switches, e.g. '-BattleElite=1 -BattleSniperTeam=1' (their purchase panel rows).
if($Extra){$a+=@($Extra -split ' '|Where-Object{$_})}
if($Map -eq 'H' -or $Map -eq 'I'){$a+=@('-BattleLeaderGuard=1','-BattleSectorMove=1','-BattleGroupFireSupport=1')}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(300000)){Stop-Process -Id $p.Id;throw 'Purchase panel shot timeout'}
$t=Get-Content $log -Raw
$lines=@($t -split "`n"|Where-Object{$_ -match 'BATTLE_BUY_PANEL_SHOT'})
$open=$lines|Where-Object{$_ -match 'stage=open' -and $_ -match 'open=1' -and $_ -match 'hits=1' -and $_ -match 'fits=1'}
$buy=$lines|Where-Object{$_ -match 'stage=buy' -and $_ -match 'bought=1' -and $_ -match 'open=1'}
$pad=$lines|Where-Object{$_ -match 'stage=pad' -and $_ -match 'moved=1' -and $_ -match 'closed=1'}
# Reservations (default on since 2026-11-23 evening; off with -BattleHireReserve=0 there are three lines): reserved through the panel, then bought by the queue.
$reserveOn=[bool]($lines|Where-Object{$_ -match 'stage=open' -and $_ -match 'reserve=1'})
$reserve=$lines|Where-Object{$_ -match 'stage=reserve ' -and $_ -match 'called=1' -and $_ -match 'chip_hits=1' -and $_ -match 'fits=1' -and $_ -match 'reserved_in_row=1'}
$reserveBuy=$lines|Where-Object{$_ -match 'stage=reservebuy' -and $_ -match 'bought=1' -and $_ -match 'queued=0'}
$reserveOk=if($reserveOn){$lines.Count -eq 5 -and $reserve -and $reserveBuy -and (Test-Path "$shot-reserved.png") -and (Test-Path "$shot-reserve-bought.png")}else{$lines.Count -eq 3}
$ok=$reserveOk -and $open -and $buy -and $pad -and (Test-Path "$shot-open.png") -and (Test-Path "$shot-bought.png") -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=[bool]$ok;exitCode=$p.ExitCode;map=$Map;resolution="${ResX}x${ResY}"}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$lines
if(!$ok){throw "Purchase panel shot failed: $log"}
Write-Output "Purchase panel shot passed: $out"
