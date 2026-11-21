[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I','J')][string]$Map='E',[string]$OutputName='',[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
# Sound and effects check (2026-11-22, BattleFXCheck): a rendered run WITH sound on a wide map. The battle is paused and, in
# front of a fixed camera, shots, bullet impacts, explosions of three sizes, the launches, the incoming whistle and the match
# cues are played. Screenshots <out>\fx-*.png, the game's audio mix recorded to <out>\fx-audio.wav (the speakers stay
# silent: the main output is turned down after the recording point), then Setup/Analyze-SoundFX.py measures the recording
# (<out>\fx-audio.png and the AUDIO_CHECK lines). Needs the build/measurement lock (MEASURE.md). About 1-2 minutes.
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project=Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if(!$OutputName){$OutputName='SoundFX-'+$Map+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss')}
$out=Join-Path $PSScriptRoot ('Benchmarks\'+$OutputName)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosplash','-BattleWideMixed','-BattleArea','-BattleFXCheck',
 '-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'fx')+'"'),('-abslog="'+$log+'"'))
$p=Start-Process -FilePath $editor -ArgumentList $a -WindowStyle Hidden -Wait -PassThru
if(!(Test-Path $log)){throw "No log (exit $($p.ExitCode)): $log"}
Select-String -LiteralPath $log -Pattern 'BATTLE_AUDIO loaded|BATTLE_FX loaded|BATTLE_AUDIO missing|FX_CHECK' | ForEach-Object { $_.Line -replace '^.*?(BATTLE_|FX_CHECK)','$1' }
$result=Select-String -LiteralPath $log -SimpleMatch 'FX_CHECK_RESULT failures=0' -Quiet
$wav=Join-Path $out 'fx-audio.wav'
if(Test-Path $wav){& python.exe -I (Join-Path $PSScriptRoot 'Analyze-SoundFX.py') $wav}
Write-Output "Output: $out"
if(!$result){throw "Sound/effects check failed (exit $($p.ExitCode)). Log: $log"}
