[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
# Runs Setup/Import-AC01Glide.py in the editor: imports the two glide clips of the AC01 suit
# (A_AC01T2_OneHand_Glide, A_AC01T2_Dual_Glide). Needs the build/measurement lock (MEASURE.md).
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$script = Join-Path $PSScriptRoot 'Import-AC01Glide.py'
$log = Join-Path $PSScriptRoot ("AC01-Glide-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".log")
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities',
    '-unattended', '-nop4', '-nosplash', '-nosound', '-RenderOffscreen', '-ResX=1280', '-ResY=900', "-abslog=`"$log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if (!(Test-Path $log) -or !(Select-String -LiteralPath $log -SimpleMatch 'AC01_GLIDE_IMPORT_OK' -Quiet)) {
    throw "Import-AC01Glide did not complete (exit $($process.ExitCode)). Log: $log"
}
(Select-String -LiteralPath $log -SimpleMatch 'AC01_GLIDE_IMPORT_OK').Line
Write-Output "Log: $log"
