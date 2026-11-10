[CmdletBinding()]
param(
    [ValidateSet(20,100,500,1000)][int]$Units = 20,
    [switch]$MassMovement,
    [ValidateSet("Open","Gate")][string]$MoveCourse,
    [ValidateRange(5,120)][int]$Seconds = 20,
    [ValidateSet('Recruit','Regular','Veteran')][string]$Preset = 'Regular',
    [ValidateSet('Recruit','Regular','Veteran')][string]$OpponentPreset = 'Regular',
    [int]$Seed = 7,
    [switch]$StableLoad,
    [switch]$NoStagger,
    [switch]$SplitDecisions,
    [switch]$Rendered,
    [switch]$FirstPerson,
    [switch]$ProfileCSV,
    [ValidateSet('Legacy','Grid','Even','Far')][string]$SenseMode = 'Legacy',
    [ValidateSet('None','RVO','Animation')][string]$DiagnosticOmit = 'None',
    [switch]$NoAnimationLOD,
    [switch]$LegacyAnimationLOD,
    [ValidateSet(0,1,4,8,16)][int]$PerceptionSlices = 0,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
if ($ProfileCSV -and !$Rendered) { throw 'ProfileCSV requires Rendered to allow the asynchronous CSV export to finish before exit.' }
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $editor)) { throw "Missing editor: $editor" }
if ($MoveCourse -and $Units -lt 500) { throw 'Movement courses support 500 and 1000 units.' }
$mode = if ($Rendered) { 'rendered' } else { 'headless' }
$schedule = if ($NoStagger) { 'batch' } else { 'staggered' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$name = "$stamp-$Units-$Preset-vs-$OpponentPreset-seed$Seed-$mode-$schedule"
if ($DiagnosticOmit -ne 'None') { $name += "-omit$DiagnosticOmit" }
$name += "-sense$SenseMode"
if ($MassMovement) { $name += '-mass' }
if ($MoveCourse) { $name += "-$MoveCourse" }
if ($SplitDecisions) { $name += '-split' }
if ($FirstPerson) { $name += '-fps' }
if ($LegacyAnimationLOD) { $name += '-legacyAnim' }
$folder = Join-Path $PSScriptRoot 'Benchmarks'
[IO.Directory]::CreateDirectory($folder) | Out-Null
$report = Join-Path $folder "$name.json"
$log = Join-Path $PSScriptRoot "Battle-Benchmark-$stamp.log"
# Record the exact local implementation, including uncommitted/new source files.
# The DLL hash identifies what actually ran even if source changes have not been built.
$projectFolder = Split-Path $project
$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $projectFolder 'Source'), (Join-Path $projectFolder 'Config') -Recurse -File | Sort-Object FullName)
$manifest = ($sourceFiles | ForEach-Object { $_.FullName.Substring($projectFolder.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }) -join "`n"
$sourceFingerprint = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($manifest)))
$binaryFingerprint = (Get-FileHash -LiteralPath (Join-Path $projectFolder 'Binaries\Win64\UnrealEditor-FutureBattle.dll') -Algorithm SHA256).Hash
$mapFingerprint = (Get-FileHash -LiteralPath (Join-Path $projectFolder 'Content\Prototype\Lvl_BattlePrototype.umap') -Algorithm SHA256).Hash
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-unattended', '-nosound', '-nosplash', '-BattleBenchmark', "-BattleUnits=$Units", "-BattleSeconds=$Seconds", "-BattlePreset=$Preset", "-BattleOpponentPreset=$OpponentPreset", "-BattleSeed=$Seed", ('-BattleReport="' + $report + '"'), ('-abslog="' + $log + '"'))
if ($DiagnosticOmit -ne 'None') { $arguments += "-BattleDiagnosticOmit=$DiagnosticOmit" }
$arguments += "-BattleSenseMode=$SenseMode"
if ($MassMovement) { $arguments += '-BattleMassMovement' }
if ($MoveCourse) { $arguments += "-BattleMoveCourse=$MoveCourse" }
if ($SplitDecisions) { $arguments += '-BattleSplitDecisions' }
if ($FirstPerson) { $arguments += '-BattleBenchmarkFPS' }
if ($ProfileCSV) { $arguments += '-BattleProfileCSV' }
if ($StableLoad) { $arguments += '-BattleStableLoad' }
if ($NoStagger) { $arguments += '-BattleNoStagger' }
if ($NoAnimationLOD) { $arguments += '-BattleNoAnimationLOD' }
if ($LegacyAnimationLOD) { $arguments += '-BattleLegacyAnimationLOD' }
if ($PerceptionSlices -gt 0) { $arguments += "-BattlePerceptionSlices=$PerceptionSlices" }
if ($Rendered) { $arguments += @('-RenderOffscreen', '-ResX=1280', '-ResY=720', '-NoVSync') } else { $arguments += '-nullrhi' }
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(($Seconds + 90) * 1000)) {
    Stop-Process -Id $process.Id
    throw "Benchmark timed out. See $log"
}
$content = Get-Content -LiteralPath $log -Raw
if ($process.ExitCode -ne 0 -or $content -notmatch 'BATTLE_BENCH_RESULT success=1' -or $content -match 'Fatal error:|Assertion failed:' -or !(Test-Path -LiteralPath $report)) { throw "Benchmark failed. See $log" }
if ($Rendered -and !(Test-Path -LiteralPath ([IO.Path]::ChangeExtension($report, '.png')))) { throw "Rendered benchmark did not save its screenshot. See $log" }
if ($ProfileCSV -and !(Test-Path -LiteralPath (Join-Path $folder ($name + '-profile.csv')))) { throw "CSV export missing. See $log" }
$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
$hardware = [ordered]@{ cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name); memory_gb = [Math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 2); gpu = @(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name) }
$result | Add-Member -NotePropertyName hardware -NotePropertyValue $hardware
$result | Add-Member -NotePropertyName source_fingerprint -NotePropertyValue $sourceFingerprint
$result | Add-Member -NotePropertyName binary_fingerprint -NotePropertyValue $binaryFingerprint
$result | Add-Member -NotePropertyName map_fingerprint -NotePropertyValue $mapFingerprint
$result | Add-Member -NotePropertyName requested_seconds -NotePropertyValue $Seconds
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $report -Encoding utf8
[pscustomobject]@{ Units=$result.unit_count; Rendered=$result.rendered; Staggered=$result.staggered; FrameMeanMS=[Math]::Round($result.frame_wall_ms.mean,3); AIPeakMS=[Math]::Round($result.combined_ai_ms_per_frame.max,3); DecisionMax=$result.decisions_per_frame.max; Shots=$result.counts.shots; Failures=$result.counts.move_failures; Report=$report } | Format-List
