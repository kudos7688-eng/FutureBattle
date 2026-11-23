[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$NoNormalMap)
$env:RB01_NORMAL_MAP = if ($NoNormalMap) { '0' } else { '1' }
# Import RB01 (Art/RB01_Tripo) as /Game/Art/RB01/SK_RB01 (Setup/Import-RB01.py). Take the lock first (MEASURE.md).
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$script = Join-Path $PSScriptRoot 'Import-RB01.py'
$log = Join-Path $PSScriptRoot ('RB01-Import-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities',
    '-unattended', '-nop4', '-nosplash', '-nosound', '-ResX=1280', '-ResY=900', '-RenderOffscreen', "-abslog=`"$log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if (!(Test-Path $log) -or !(Select-String -LiteralPath $log -SimpleMatch 'RB01_IMPORT_OK' -Quiet)) {
    throw "Import-RB01 did not complete (exit $($process.ExitCode)). Log: $log"
}
Write-Output "RB01 imported. Log: $log"
