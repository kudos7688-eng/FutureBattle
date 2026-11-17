[CmdletBinding()]
param([int[]]$Seeds=(1..20),[string]$OutputFolder='', [switch]$BaselineOnly, [switch]$VariantsOnly, [switch]$Rendered,
 [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
if(!$OutputFolder){$OutputFolder=Join-Path $PSScriptRoot ('Benchmarks/T2b-'+(Get-Date -Format yyyyMMdd-HHmmss))}
[IO.Directory]::CreateDirectory($OutputFolder)|Out-Null
$OutputFolder=[IO.Path]::GetFullPath($OutputFolder)
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project=[IO.Path]::GetFullPath("$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject")
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$hash=(Get-FileHash $dll -Algorithm SHA256).Hash
$lockPath=Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'
$lock=$null
try {
 $lock=[IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue){throw 'An Unreal Editor is running; measurement requires exclusive use.'}
 if($VariantsOnly){
  $previous=Get-Content (Join-Path $OutputFolder 'manifest.json') -Raw|ConvertFrom-Json
  if($previous.dll_sha256 -ne $hash){throw 'Variants must use the same DLL as the OFF regression'}
 }
 @{dll_sha256=$hash;seeds=$Seeds;resolution='1920x1080';count=500;actors=50;mass=450;headless=(!$Rendered);created=(Get-Date -Format o)}|ConvertTo-Json|Set-Content (Join-Path $OutputFolder 'manifest.json')
 function Invoke-T2b([int]$seed,[string]$mode,[string]$condition) {
  if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed before run'}
  $name="seed$seed-$mode-$condition"; $log=Join-Path $OutputFolder "$name.log"; $capture=Join-Path $OutputFolder "$name.png"
  $argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts','-BattleFieldDiagnostics',"-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",'-T2b-Metrics','-T2a-Allocation=Off',('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
  if($condition -eq 'Off'){$argsList+='-T2b-TurnToFace=Off'}else{
   $rate=if($condition -eq 'Rate540'){540}else{1080}
   $argsList+=@('-T2b-TurnToFace=On',"-T2b-TurnRate=$rate")
  }
  if($Rendered){$argsList+='-RenderOffscreen'}else{$argsList+=@('-nullrhi','-T2b-Headless')}
  $p=Start-Process $editor -ArgumentList $argsList -WindowStyle Hidden -PassThru
  if(!$p.WaitForExit(600000)){Stop-Process -Id $p.Id;throw "Timeout $log"}
  $line=(Select-String $log -Pattern 'FIELD_TRIAL mode='|Select-Object -Last 1).Line
  if($p.ExitCode -ne 0 -or $line -notmatch 'success=1'){throw "Failed $log $line"}
  if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed during run'}
  Add-Content (Join-Path $OutputFolder 'results.txt') "$name $line"
  Write-Output "$name complete"
 }
 if(!$VariantsOnly){foreach($seed in $Seeds){foreach($mode in 'Individual','Squad'){Invoke-T2b $seed $mode 'Off'}}}
 $python=Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
 & $python "$PSScriptRoot/Check-T2bRegression.py" $OutputFolder --expected ($Seeds.Count*2)
 if($LASTEXITCODE -ne 0){throw 'OFF regression failed; variants are not allowed'}
 if(!$BaselineOnly){foreach($seed in $Seeds){foreach($condition in 'Rate540','Rate1080'){Invoke-T2b $seed 'Squad' $condition}}}
} finally {if($lock){$lock.Dispose()}}
