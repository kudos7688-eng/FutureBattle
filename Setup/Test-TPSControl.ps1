[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. "$PSScriptRoot/Get-MeasurementMachine.ps1"
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$folder=Join-Path $root 'Setup/Benchmarks/TPS-Control-20261111'
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
function Manifest($name,$state) {
 $script:manifest.status=$state
 $script:manifest | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 (Join-Path $folder "$name-manifest.json")
}
foreach($name in 'tps','integration','large-field') {
 $script:manifest=@{machine=Get-MeasurementMachine; dll_sha256=(Get-FileHash $dll).Hash; started=(Get-Date).ToString('o');status='running'}
 Manifest $name 'running'
 if($name -eq 'tps') {
  $log=Join-Path $folder 'tps.log'
  $project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
  $args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-nullrhi','-unattended','-nosound','-BattleSmokeTest','-BattleSmokeSuite=TPS',('-abslog="'+$log+'"'))
  $p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $args -WindowStyle Hidden -PassThru
  if(!$p.WaitForExit(180000)){throw 'TPS test timed out'}
  $text=Get-Content $log -Raw
  if($p.ExitCode -ne 0 -or $text -notmatch 'BATTLE_TPS_RESULT failures=0' -or $text -match 'BATTLE_TEST FAIL|Assertion failed:|Fatal error:'){throw "TPS failed: $log"}
  $text -split "`n" | Where-Object {$_ -match 'BATTLE_TEST|BATTLE_TPS_RESULT'} | Write-Output
 } elseif($name -eq 'integration') { & "$PSScriptRoot/Test-Battle.ps1" *> (Join-Path $folder 'integration.txt') }
 else { & "$PSScriptRoot/Test-LargeField.ps1" -Rendered *> (Join-Path $folder 'large-field.txt') }
 Manifest $name 'passed'
 Write-Output "$name PASS"
}
