[CmdletBinding()]
param([string]$OutputFolder='', [int]$Samples=1000000,[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
if(!$OutputFolder){$OutputFolder=Join-Path $PSScriptRoot ('Benchmarks/'+(Get-Date -Format yyyyMMdd-HHmmss)+'-T4')}
[IO.Directory]::CreateDirectory($OutputFolder)|Out-Null
$OutputFolder=[IO.Path]::GetFullPath($OutputFolder)
$project=[IO.Path]::GetFullPath("$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject")
$argsList=@(('"'+$project+'"'),'-run=BattleConeTrial','-nullrhi','-unattended','-nosound',"-Samples=$Samples",('-Output="'+$OutputFolder+'"'),('-abslog="'+$OutputFolder+'/engine.log"'))
$p=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $argsList -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(300000)){Stop-Process -Id $p.Id;throw 'Cone test timeout'}
if($p.ExitCode -ne 0 -or !(Select-String -Path "$OutputFolder/engine.log" -Pattern 'CONE_TRIAL .*success=1')){throw 'Cone test failed'}
Write-Output "T4 output: $OutputFolder"
