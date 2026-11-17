[CmdletBinding()]
param([int[]]$Seeds=(1..20),[string]$OutputFolder='', [switch]$RecordOnly,[switch]$ReplayOnly,[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
if(!$OutputFolder){$OutputFolder=Join-Path $PSScriptRoot ('Benchmarks/'+(Get-Date -Format yyyyMMdd-HHmmss)+'-T1')}
[IO.Directory]::CreateDirectory($OutputFolder)|Out-Null
$OutputFolder=[IO.Path]::GetFullPath($OutputFolder)
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project=[IO.Path]::GetFullPath("$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject")
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll -Algorithm SHA256).Hash
function Invoke-Trial([string]$name,[string[]]$extra,[string]$pattern,[int]$expected=1){
 $log=Join-Path $OutputFolder "$name.log"
 $capture=Join-Path $OutputFolder "$name.png"
 $arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts',('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))+$extra
 $p=Start-Process $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
 if(!$p.WaitForExit(600000)){Stop-Process -Id $p.Id;throw "Timeout $log"}
 $lines=@(Select-String -Path $log -Pattern $pattern | ForEach-Object {$_.Line})
 if($p.ExitCode -ne 0 -or $lines.Count -ne $expected -or @($lines | Where-Object {$_ -notmatch 'success=1'}).Count){throw "Failed $log"}
 if((Get-FileHash $dll -Algorithm SHA256).Hash -ne $hash){throw 'DLL changed during run'}
 $lines | Add-Content (Join-Path $OutputFolder 'results.txt')
 Write-Output "$name completed ($($lines.Count) conditions)"
}
@{dll_sha256=$hash;seeds=$Seeds;resolution='1920x1080';count=500;actors=50;mass=450;runs=2;created=(Get-Date -Format o)}|ConvertTo-Json|Set-Content (Join-Path $OutputFolder 'manifest.json')
foreach($seed in $Seeds){
 if(!$ReplayOnly){
  $modes=if($seed%2){@('Individual','Squad')}else{@('Squad','Individual')}
  foreach($mode in $modes){
   $record=Join-Path $OutputFolder "seed$seed-$mode.fbr"
   Invoke-Trial "seed$seed-$mode" @("-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",'-BattleFieldDiagnostics',('-BattleFieldRecord="'+$record+'"')) 'FIELD_TRIAL mode='
  }
 }
 if(!$RecordOnly){
  $a=Join-Path $OutputFolder "seed$seed-Individual.fbr";$b=Join-Path $OutputFolder "seed$seed-Squad.fbr"
  Invoke-Trial "seed$seed-replay" @("-BattleFieldSeed=$seed",'-BattleSquadTrial=Individual','-BattleReplayBatch',('-BattleFieldReplay="'+$b+'"'),('-BattleReplayReference="'+$a+'"')) 'FIELD_REPLAY arm=' 32
 }
}
Write-Output "T1 output: $OutputFolder"
