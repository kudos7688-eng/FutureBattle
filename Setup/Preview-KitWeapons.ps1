[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$Grip = '0,0,0,0,0,0'
)
# Visual fixture only. Does not build, generate maps, or change the evaluation scripts.
$ErrorActionPreference = 'Stop'
$numbers = $Grip.Split(',')
if ($numbers.Count -ne 6) { throw 'Grip must be x,y,z,pitch,yaw,roll' }
foreach ($part in $numbers) {
    $value = 0.0
    if (![double]::TryParse($part, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$value) -or
        [double]::IsNaN($value) -or [double]::IsInfinity($value)) { throw 'Grip values must be finite numbers' }
}
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Missing editor: $editor" }
$prefix = Join-Path $PSScriptRoot ('Kit-Weapons-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$log = "$prefix.log"
$arguments = @(
    ('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-unattended', '-nosound', '-nosplash',
    '-RenderOffscreen', '-ResX=1280', '-ResY=720', '-BattleSmokeTest', '-BattleSmokeSuite=Weapons',
    '-BattleFirstPerson', '-BattleKitLook=1', ('-BattleKitGrip="' + $Grip + '"'),
    ('-BattleWeaponPreview="' + $prefix + '"'), ('-abslog="' + $log + '"')
)
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(180000)) {
    Stop-Process -Id $process.Id
    throw "Kit preview timed out. See $log"
}
if (!(Test-Path -LiteralPath $log -PathType Leaf)) { throw "No preview log (exit $($process.ExitCode)): $log" }
$text = Get-Content -LiteralPath $log -Raw
$text -split "`n" | Where-Object { $_ -match 'BATTLE_(KITLOOK|TEST|SMOKE_RESULT)' } | Write-Output
if ($process.ExitCode -ne 0 -or $text -notmatch 'BATTLE_SMOKE_RESULT failures=0' -or $text -match 'BATTLE_TEST FAIL|Fatal error:|Assertion failed:') {
    throw "Kit preview failed (missing kit meshes also fail). See $log"
}
foreach ($index in 0..9) {
    foreach ($view in 'fps','hand') {
        $shot = '{0}-kit-{1:00}-{2}.png' -f $prefix,$index,$view
        if (!(Test-Path -LiteralPath $shot -PathType Leaf)) { throw "Missing screenshot: $shot" }
    }
}
Write-Output "Kit preview passed (20 screenshots): $prefix-kit-*.png"
