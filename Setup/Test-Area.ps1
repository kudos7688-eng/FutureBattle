[CmdletBinding()]
param([string]$OutputName='pilot',[double]$BaseRifleDamage=0,[switch]$Rendered,[ValidateSet(0,1)][int]$DisplayLOD=1,[ValidateSet(0,1)][int]$FlankRelaxed=0,[ValidateRange(0,3)][int]$FlankTeams=3,[ValidateSet(0,1)][int]$CoverActions=0,[ValidateSet(0,1)][int]$FlankCoordination=0,[ValidateSet(0,1)][int]$AreaCompositionPlanner=0,[ValidateRange(0,3)][int]$AreaCompositionPlannerTeams=0)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$out=Join-Path $PSScriptRoot "Benchmarks/Area-20261112/$OutputName"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_A','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleArea','-BattleAreaSmoke',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
$arguments+=@("-BattleAreaCompositionPlanner=$AreaCompositionPlanner","-BattleAreaCompositionPlannerTeams=$AreaCompositionPlannerTeams")
$arguments+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams","-BattleCoverActions=$CoverActions","-BattleFlankCoordination=$FlankCoordination")
if($BaseRifleDamage -gt 0){$arguments+="-BattleBaseRifleDamage=$BaseRifleDamage"}
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
@{compositionPlanner=@{stage=2;purchase="planner_when_enabled";enabled=$AreaCompositionPlanner;teams=$AreaCompositionPlannerTeams};machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$arguments;map=(Get-FileHash (Join-Path $root 'Game/FutureBattle/Content/Prototype/Lvl_BattleWide_A.umap')).Hash} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id;throw "Timed out: $log"}
$t=Get-Content $log -Raw
# Planner diagnostics: preserve actual resolved definitions and the first pre-purchase input for each observed team.
$compositionManifest=Get-Content (Join-Path $out 'manifest.json') -Raw | ConvertFrom-Json
$compositionResolved=@($t -split "`n" | Where-Object {$_ -match 'BATTLE_COMPOSITION_CONFIG|BATTLE_COMPOSITION_DEF|BATTLE_COMPOSITION_RULES'})
foreach($compositionTeam in 0,1){$compositionResolved+=@($t -split "`n" | Where-Object {$_ -match "BATTLE_COMPOSITION_INPUT .*team=$compositionTeam "} | Select-Object -First 1)}
$compositionManifest | Add-Member -NotePropertyName compositionResolvedLog -NotePropertyValue $compositionResolved -Force
$compositionManifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$pass=$p.ExitCode -eq 0 -and $t -match 'BATTLE_AREA_RESULT failures=0' -and $t -notmatch 'BATTLE_AREA_CHECK FAIL|Fatal error:|Assertion failed:'
@{passed=$pass;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash;checks=([regex]::Matches($t,'BATTLE_AREA_CHECK PASS')).Count}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json')
$t -split "`n"|Where-Object{$_ -match 'BATTLE_AREA'}|Write-Output
if(!$pass){throw "Test failed: $log"}
if($Rendered -and !(Test-Path (Join-Path $out 'screen-restored.png'))){throw 'Missing restored screen'}

