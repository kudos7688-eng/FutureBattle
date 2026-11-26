[CmdletBinding()]
# Rendered section-order UI capture. Does not build the editor.
param([string]$Name='planpanel-r1',[int]$ResX=1920,[int]$ResY=1080,[string]$Extra='')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/PlanPanelShot-20261126/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$machine=Get-MeasurementMachine
@{machine=$machine;map='J';resolution="${ResX}x${ResY}";extra=$Extra}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding UTF8
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$log=Join-Path $out 'run.log'
$shot=Join-Path $out 'shot'
$a=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_J','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ForceRes',"-ResX=$ResX","-ResY=$ResY",
 '-BattleWideMixed','-BattleArea','-BattleCoverActions=1','-BattleFlankCoordination=1','-BattleAreaPointSupply=1','-BattleFollowerAnimSkip=1',
 '-BattleFrontPlan=1','-BattleFrontPlanTeam=0','-BattleFrontPlanPlayer=1','-BattleFrontPlanFocus2=1','-BattleHireReserve=1','-BattleFrontSectionOrders=1','-BattleStartRunning',
 ('-BattlePlanPanelShot="'+$shot+'"'),('-abslog="'+$log+'"'))
if($Extra){$a+=@($Extra -split ' '|Where-Object{$_})}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(300000)){Stop-Process -Id $p.Id;throw 'Plan panel shot timeout'}
$t=Get-Content $log -Raw
$lines=@($t -split "`n"|Where-Object{$_ -match 'BATTLE_PLAN_PANEL_SHOT'})
$draw=$lines|Where-Object{$_ -match 'stage=draw first=5 last=6 sections=([3-5])'}
$sections=0
if(($draw -join '') -match 'sections=([3-5])'){$sections=[int]$Matches[1]}
$map=$lines|Where-Object{$_ -match "stage=map labels=$sections"}
$menu=$lines|Where-Object{$_ -match 'stage=menu section=1'}
$panel=$lines|Where-Object{$_ -match "stage=panel open=1 buttons=$($sections*7) fits=1"}
$ok=$p.ExitCode -eq 0 -and $sections -ge 3 -and $draw -and $map -and $menu -and $panel -and (Test-Path "$shot-map.png") -and (Test-Path "$shot-menu.png") -and (Test-Path "$shot-panel.png") -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=[bool]$ok;exitCode=$p.ExitCode;map='J';resolution="${ResX}x${ResY}"}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding UTF8
$lines
if(!$ok){throw "Plan panel shot failed: $log"}
Write-Output "Plan panel shot passed: $out"
