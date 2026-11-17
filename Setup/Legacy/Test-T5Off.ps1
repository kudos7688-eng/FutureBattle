[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputFolder,[int[]]$Seeds=(1..20))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$out=[IO.Path]::GetFullPath($OutputFolder);[IO.Directory]::CreateDirectory($out)|Out-Null
$file=Join-Path $out 'manifest.json';if(Test-Path $file){throw 'Use a fresh output folder'}
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$m=@{test='T5 default OFF regression';machine=(Get-MeasurementMachine);dll_sha256=(Get-FileHash $dll).Hash;created=(Get-Date -Format o);seeds=$Seeds;status='running'}
$m|ConvertTo-Json -Depth 8|Set-Content $file
$lock=[IO.File]::Open((Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
 if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue){throw 'Another Unreal process is running'}
 & (Join-Path $PSScriptRoot 'Test-Battle.ps1') *> (Join-Path $out 'smoke.txt')
 if(!(Select-String (Join-Path $out 'smoke.txt') -Pattern 'BATTLE_SMOKE_RESULT failures=0')){throw 'Smoke failed'}
}finally{$lock.Dispose()}
& (Join-Path $PSScriptRoot 'Test-T2b.ps1') -BaselineOnly -Seeds $Seeds -OutputFolder (Join-Path $out 'battles') *> (Join-Path $out 'off-progress.txt')
$r=Get-Content (Join-Path $out 'battles/regression.json') -Raw|ConvertFrom-Json
if($r.matches -ne ($Seeds.Count*2) -or (Get-FileHash $dll).Hash -ne $m.dll_sha256){throw 'Regression or DLL mismatch'}
$m.matches=$r.matches;$m.smoke_pass=(Select-String (Join-Path $out 'smoke.txt') -Pattern 'BATTLE_TEST PASS').Count;$m.status='complete';$m|ConvertTo-Json -Depth 8|Set-Content $file
Write-Output "T5 OFF: $($m.matches)/$($Seeds.Count*2); smoke $($m.smoke_pass) PASS"
