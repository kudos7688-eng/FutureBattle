[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
# Runs Setup/Refresh-AC01Body.py in the editor: replaces the AC01 body geometry (with the modelled joint sleeves)
# and keeps the installed material and skeleton. Needs the build/measurement lock (MEASURE.md).
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$script = Join-Path $PSScriptRoot 'Refresh-AC01Body.py'
$log = Join-Path $PSScriptRoot ("AC01-RefreshBody-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".log")
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities',
    '-unattended', '-nop4', '-nosplash', '-nosound', '-RenderOffscreen', '-ResX=1280', '-ResY=900', "-abslog=`"$log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if (!(Test-Path $log) -or !(Select-String -LiteralPath $log -SimpleMatch 'AC01_BODY_REFRESH_OK' -Quiet)) {
    throw "Refresh-AC01Body did not complete (exit $($process.ExitCode)). Log: $log"
}
(Select-String -LiteralPath $log -SimpleMatch 'AC01_BODY_REFRESH_OK').Line
Write-Output "Log: $log"
