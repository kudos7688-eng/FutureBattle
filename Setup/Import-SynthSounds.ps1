[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
# Imports the synthesized sounds (Setup/SynthAudio/*.wav, made by Setup/Make-SynthSounds.py) into /Game/Audio/Battle as
# SoundWaves with UE's ImportAssets commandlet (existing ones are replaced). Needs the build/measurement lock (MEASURE.md).
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$waves = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'SynthAudio') -Filter 'SW_Synth_*.wav' | Sort-Object Name)
if ($waves.Count -eq 0) { throw 'No Setup/SynthAudio/SW_Synth_*.wav (run Setup/Make-SynthSounds.py first)' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$settings = Join-Path $env:TEMP "FutureBattle-SynthImport-$stamp.json"
$log = Join-Path $PSScriptRoot "SynthImport-$stamp.log"
$json = [pscustomobject]@{ ImportGroups = @([pscustomobject]@{
    GroupName = 'BattleSynth'; Filenames = @($waves | ForEach-Object { $_.FullName.Replace('\', '/') })
    DestinationPath = '/Game/Audio/Battle'; FactoryName = 'SoundFactory'; bReplaceExisting = $true; bSkipReadOnly = $false }) } | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText($settings, $json, (New-Object System.Text.UTF8Encoding($false)))
$arguments = @("`"$project`"", '-run=ImportAssets', "-importsettings=`"$settings`"", '-nosourcecontrol',
    '-unattended', '-nop4', '-nosplash', '-nosound', "-abslog=`"$log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
$content = Join-Path $root 'Game\FutureBattle\Content\Audio\Battle'
$missing = @($waves | Where-Object { !(Test-Path -LiteralPath (Join-Path $content ($_.BaseName + '.uasset'))) })
if ($process.ExitCode -ne 0 -or $missing.Count -gt 0) {
    throw "Import-SynthSounds failed (exit $($process.ExitCode), missing $($missing.BaseName -join ', ')). Log: $log"
}
Write-Output "SYNTH_IMPORT_OK sounds=$($waves.Count) dest=/Game/Audio/Battle"
Write-Output "Log: $log"
