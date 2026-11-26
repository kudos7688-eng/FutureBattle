[CmdletBinding()]
param([string]$Name='coverpeek-r1',[switch]$Rendered)
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/FlankStages-20261112/$Name"
if(Test-Path $out){throw 'Output exists'}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_C','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleCoverPeekTrial','-BattleStances=1','-BattleCoverActions=1','-BattleCoverFire=1','-BattleCoverPeekSight=1','-BattleCoverValidation=1','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0','-BattleSightRange=10000',('-BattleSightExperiment="'+$out+'"'),('-abslog="'+$log+'"'))
if($Rendered){$a+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720')}else{$a+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$a}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
$timedOut=!$p.WaitForExit(180000)
if($timedOut){Stop-Process -Id $p.Id;$p.WaitForExit()}
$t=if(Test-Path $log){Get-Content $log -Raw}else{''}
$checks=@([regex]::Matches($t,'BATTLE_COVERPEEK_CHECK name=(visible|lost) ok=([01])[^\r\n]*')|ForEach-Object{@{name=$_.Groups[1].Value;passed=$_.Groups[2].Value -eq '1';line=$_.Value}})
$scenes=@{}
foreach($m in [regex]::Matches($t,'BATTLE_COVERPEEK_DIAG name=(\w+) (peekwhy_\w+)=(\d+)')){
 $scene=$m.Groups[1].Value
 if(!$scenes.ContainsKey($scene)){$scenes[$scene]=@{cover_diagnostics=@{}}}
 $scenes[$scene].cover_diagnostics[$m.Groups[2].Value]=[int]$m.Groups[3].Value
}
$ok=!$timedOut -and $p.ExitCode -eq 0 -and $t -match 'BATTLE_COVERPEEK_RESULT failures=0' -and $checks.Count -eq 2 -and @($checks|Where-Object{$_.name -eq 'visible'}).Count -eq 1 -and @($checks|Where-Object{$_.name -eq 'lost'}).Count -eq 1 -and @($checks|Where-Object{!$_.passed}).Count -eq 0 -and $t -match 'BATTLE_COVERPEEK_CHECK name=undetected info' -and $t -notmatch 'BATTLE_COVERPEEK_CHECK[^\r\n]*ok=0|Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode;timedOut=$timedOut;checks=$checks;scenes=$scenes}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n"|Where-Object{$_ -match 'BATTLE_COVERPEEK_(CHECK|RESULT|DIAG)'}
if(!$ok){throw "Cover peek failed: $log"}
