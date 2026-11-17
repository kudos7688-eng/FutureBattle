[CmdletBinding()]
param([int[]]$Seeds=(1..10),[string]$OutputFolder='',[int]$Conditions=30,[switch]$VisualQA,[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
if(!$OutputFolder){$OutputFolder=Join-Path $PSScriptRoot ('Benchmarks/T5-'+(Get-Date -Format yyyyMMdd-HHmmss))}
[IO.Directory]::CreateDirectory($OutputFolder)|Out-Null
$OutputFolder=[IO.Path]::GetFullPath($OutputFolder)
$project=[IO.Path]::GetFullPath("$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject")
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$hash=(Get-FileHash $dll -Algorithm SHA256).Hash
$lock=$null
try {
 $lock=[IO.File]::Open((Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue){throw 'An Unreal Editor is running; T5 requires exclusive measurement.'}
 if(Test-Path (Join-Path $OutputFolder 'manifest.json')){throw 'Use a fresh output folder to avoid overwriting evidence'}
 $manifest=@{dll_sha256=$hash;seeds=$Seeds;conditions=$Conditions;resolution='1920x1080';count=500;actors=50;mass=450;created=(Get-Date -Format o);status='running';records=@{}}
 $manifest.machine=Get-MeasurementMachine
 Copy-Item -LiteralPath $dll -Destination (Join-Path $OutputFolder 'UnrealEditor-FutureBattle.dll')
 foreach($seed in $Seeds){
  $record=[IO.Path]::GetFullPath("$PSScriptRoot/Benchmarks/T1-camera-20261106/seed$seed-Individual.fbr")
  $manifest.records[$record]=(Get-FileHash $record -Algorithm SHA256).Hash
 }
 $manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputFolder 'manifest.json')
 foreach($seed in $Seeds){
  if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed'}
  $record=[IO.Path]::GetFullPath("$PSScriptRoot/Benchmarks/T1-camera-20261106/seed$seed-Individual.fbr")
  $capture=Join-Path $OutputFolder "seed$seed.png"; $log=Join-Path $OutputFolder "seed$seed.log"
  $argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts',"-BattleFieldSeed=$seed",'-BattleSquadTrial=Individual','-T5-Replay',"-T5-Conditions=$Conditions",'-T2a-Allocation=Off','-T2b-TurnToFace=Off',('-BattleFieldReplay="'+$record+'"'),('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
  if($VisualQA){$argsList+='-T5-VisualQA'}
  $p=Start-Process $editor -ArgumentList $argsList -WindowStyle Hidden -PassThru
  if(!$p.WaitForExit(900000)){Stop-Process -Id $p.Id;throw "Timeout $log"}
  $lines=@(Select-String $log -Pattern 'T5_REPLAY seed='|ForEach-Object {$_.Line})
  if($p.ExitCode -ne 0 -or $lines.Count -ne $Conditions -or @($lines|Where-Object {$_ -notmatch 'success=1'}).Count){throw "Failed $log"}
  if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed during run'}
  $lines|Add-Content (Join-Path $OutputFolder 'results.txt');Write-Output "seed$seed complete: $Conditions conditions"
 }
 $manifest.status='complete';$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputFolder 'manifest.json')
}finally{if($lock){$lock.Dispose()}}
