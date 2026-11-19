[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'Game\FutureBattle\FutureBattle.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$report = Join-Path $root 'Art\AC01_Tripo_v2\unreal-validation.json'
foreach ($step in 'Import-AC01T2', 'Verify-AC01T2-Unreal') {
    $script = Join-Path $PSScriptRoot "$step.py"
    $log = Join-Path $PSScriptRoot ("AC01T2-" + $step + "-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".log")
    $arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities',
        '-unattended', '-nop4', '-nosplash', '-nosound', '-ResX=1280', '-ResY=900', "-abslog=`"$log`"")
    if ($step -eq 'Import-AC01T2') { $arguments += '-RenderOffscreen' }
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    $marker = if ($step -eq 'Import-AC01T2') { 'AC01T2_IMPORT_OK' } else { 'AC01T2_PIE_CHECK' }
    if (!(Test-Path $log) -or !(Select-String -LiteralPath $log -SimpleMatch $marker -Quiet)) {
        throw "$step did not complete (exit $($process.ExitCode)). Log: $log"
    }
    $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    if ($step -eq 'Import-AC01T2') {
        $clips = @($result.animations.PSObject.Properties)
        if (!$result.import_ok -or $clips.Count -ne 16 -or @($clips | Where-Object { !$_.Value.skeleton_ok }).Count) {
            throw "AC01T2 import validation failed. See $report"
        }
    } elseif (!$result.play_in_editor_verified -or @($result.pie_animation_moves.PSObject.Properties | Where-Object { !$_.Value }).Count) {
        throw "AC01T2 preview validation failed. See $report"
    }
}
Write-Output "AC01T2 imported and previewed. $report"
