[CmdletBinding()]
param(
 [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
 [ValidateSet('SquadTactics','Tactical','Squad','All','Delegation')][string]$Suite = 'SquadTactics',
 [switch]$LargeField, [switch]$Rendered,
 [ValidateSet(0,1)][int]$Enabled = 1,
 [int]$Width = 1280, [int]$Height = 720,
 [ValidateRange(3,90)][int]$Seconds = 30,
 [string]$OutputName = 'small-on'
)
$ErrorActionPreference = 'Stop'
if ($OutputName -notmatch '^[A-Za-z0-9_-]+$') { throw 'OutputName must be a simple identifier' }
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$out = Join-Path $PSScriptRoot ('Benchmarks\SquadTactics-ST01-20261111\' + $OutputName)
if (Test-Path $out) { throw "Output already exists: $out" }
New-Item -ItemType Directory -Path $out -Force | Out-Null
$dll = Join-Path (Split-Path $project) 'Binaries\Win64\UnrealEditor-FutureBattle.dll'
$manifest = [ordered]@{ machine = Get-MeasurementMachine; suite = $Suite; large = [bool]$LargeField; rendered = [bool]$Rendered; enabled = $Enabled; seconds = $Seconds; width = $Width; height = $Height; dll = (Get-FileHash $dll -Algorithm SHA256).Hash; started = [DateTime]::UtcNow.ToString('o') }
$manifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$map = if ($LargeField) { '/Game/Prototype/Lvl_BattleField500' } else { '/Game/Prototype/Lvl_BattlePrototype' }
$log = Join-Path $out 'run.log'
$arguments = @(('"'+$project+'"'),$map,'-game','-unattended','-nosound','-nosplash','-BattleSmokeTest','-BattleTacticalObserve',"-BattleSquadTactics=$Enabled", "-BattleSTObserveSeconds=$Seconds",('-abslog="'+$log+'"'))
if ($Suite -ne 'All') { $arguments += "-BattleSmokeSuite=$Suite" }
if ($Rendered) { $arguments += @('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleSTScreens="'+(Join-Path $out 'screen')+'"')) }
else { $arguments += '-nullrhi' }
$process = Start-Process (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(240000)) { Stop-Process -Id $process.Id; throw "Timed out: $log" }
$content = Get-Content $log -Raw
$passed = $content -match 'BATTLE_(SMOKE|DELEGATION)_RESULT failures=0' -and $content -notmatch 'BATTLE_TEST FAIL|Fatal error:|Assertion failed:' -and $process.ExitCode -eq 0
$result = [ordered]@{ passed = $passed; exitCode = $process.ExitCode; checks = ([regex]::Matches($content,'BATTLE_TEST PASS')).Count; dll = (Get-FileHash $dll -Algorithm SHA256).Hash }
$result | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$content -split "`n" | Where-Object { $_ -match 'BATTLE_TEST|BATTLE_SMOKE_RESULT|BATTLE_DELEGATION_RESULT' } | Write-Output
if (!$passed) { throw "Test failed: $log" }
if ($Rendered -and $Suite -eq 'SquadTactics' -and !(Test-Path (Join-Path $out 'screen-rts.png'))) { throw 'Missing screenshot' }
Write-Output "Evidence: $out"
