[CmdletBinding()]
param([string]$OutputFolder = "$PSScriptRoot/Benchmarks/AllyDelegation-20261111", [ValidateSet('small','wide720','wide1080','integration')][string[]]$Cases = @('small','wide720','wide1080','integration'))
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$OutputFolder = [IO.Path]::GetFullPath($OutputFolder)
New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
$project = Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$dll = Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
foreach ($name in $Cases) {
    $manifest = @{ machine = Get-MeasurementMachine; dll_sha256 = (Get-FileHash $dll).Hash; started = (Get-Date).ToString('o'); status = 'running' }
    $manifestPath = Join-Path $OutputFolder "$name-manifest.json"
    $manifest | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 $manifestPath
    if ($name -eq 'integration') {
        & "$PSScriptRoot/Test-Battle.ps1" *> (Join-Path $OutputFolder 'integration.txt')
    } else {
        $wide = $name -ne 'small'
        $map = if ($wide) { '/Game/Prototype/Lvl_BattleField500' } else { '/Game/Prototype/Lvl_BattlePrototype' }
        $log = Join-Path $OutputFolder "$name.log"
        $arguments = @(('"'+$project+'"'), $map, '-game', '-unattended', '-nosound', '-BattleSmokeTest', '-BattleSmokeSuite=Delegation', ('-abslog="'+$log+'"'))
        if ($wide) {
            $arguments += @('-BattleScaleUnits=500', '-RenderOffscreen', '-Windowed', '-ForceRes', ('-BattleDelegationScreens="'+(Join-Path $OutputFolder $name)+'"'))
            $arguments += if ($name -eq 'wide720') { @('-ResX=1280','-ResY=720') } else { @('-ResX=1920','-ResY=1080') }
        } else { $arguments += '-nullrhi' }
        $process = Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
        if (!$process.WaitForExit(180000)) { throw "Delegation test timed out: $name, process $($process.Id)" }
        $logText = Get-Content $log -Raw
        if ($process.ExitCode -ne 0 -or $logText -notmatch 'BATTLE_DELEGATION_RESULT failures=0' -or $logText -match 'BATTLE_TEST FAIL|Assertion failed:|Fatal error:') { throw "Delegation test failed: $log" }
        if ($wide -and !(Test-Path (Join-Path $OutputFolder "$name-rts.png"))) { throw "Screenshot missing: $name" }
        if ($wide) {
            $png = [IO.File]::ReadAllBytes((Join-Path $OutputFolder "$name-rts.png"))
            $width = $png[16]*16777216 + $png[17]*65536 + $png[18]*256 + $png[19]
            $height = $png[20]*16777216 + $png[21]*65536 + $png[22]*256 + $png[23]
            $expectedWidth = if ($name -eq 'wide720') { 1280 } else { 1920 }
            $expectedHeight = if ($name -eq 'wide720') { 720 } else { 1080 }
            if ($width -ne $expectedWidth -or $height -ne $expectedHeight) { throw "Wrong screenshot size: ${width}x${height}" }
            $manifest.width = $width; $manifest.height = $height
        }
    }
    $manifest.status = 'passed'
    $manifest | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 $manifestPath
    Write-Output "$name PASS"
}
