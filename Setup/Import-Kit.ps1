[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
# Runs Setup/Import-Kit.py in the editor: imports the asset kit (Art/Kit: surfaces, weapons, props) into /Game/Kit.
# Display assets only. Needs the build/measurement lock (MEASURE.md) and a built editor DLL in this worktree.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$script = Join-Path $PSScriptRoot 'Import-Kit.py'
$log = Join-Path $PSScriptRoot ("Kit-Import-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".log")
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities',
    '-unattended', '-nop4', '-nosplash', '-nosound', '-RenderOffscreen', '-ResX=1280', '-ResY=900', "-abslog=`"$log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if (!(Test-Path $log) -or !(Select-String -LiteralPath $log -SimpleMatch 'KIT_IMPORT_DONE' -Quiet)) {
    throw "Import-Kit did not complete (exit $($process.ExitCode)). Log: $log"
}
(Select-String -LiteralPath $log -SimpleMatch 'KIT_IMPORT_DONE').Line
Write-Output "Log: $log"
