[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputFolder,
 [ValidateSet(40,500,1000)][int]$Units=500,
 [ValidateSet('Individual','Leader')][string]$Decision='Leader',
 [ValidateSet('RTS','TPS','FPS')][string]$View='RTS',
 [ValidateSet('Open','Cross','Gate')][string]$Layout='Open',
 [int]$Seed=1,[double]$Warmup=10,[double]$Duration=60,[switch]$Stable,[switch]$NullRHI,[switch]$Capture,[switch]$Functional)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$out=[IO.Path]::GetFullPath($OutputFolder)
if(Test-Path (Join-Path $out 'manifest.json')) { throw 'Use a fresh output folder' }
if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { throw 'An Unreal process is already running' }
$lock=[IO.File]::Open((Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
[IO.Directory]::CreateDirectory($out)|Out-Null
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleLeaderUnits=$Units",('-abslog="'+$log+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'),"-P3Decision=$Decision","-P3View=$View","-P3Layout=$Layout","-P3Seed=$Seed","-P3Warmup=$Warmup","-P3Duration=$Duration",'-ResX=1920','-ResY=1080','-Windowed','-ForceRes','-NoVSync')
if($NullRHI) { $args+='-nullrhi' } else { $args+='-RenderOffscreen' }
if($Stable) { $args+='-P3Stable' }
if($Capture) { $args+='-P3Capture' }
if($Functional) { $args+='-P3Functional' }
$m=@{schema=1;status='running';machine=(Get-MeasurementMachine);dll_sha256=(Get-FileHash $dll).Hash;created=(Get-Date -Format o);units=$Units;decision=$Decision;view=$View;layout=$Layout;seed=$Seed;warmup=$Warmup;duration=$Duration;stable=[bool]$Stable;nullrhi=[bool]$NullRHI;arguments=$args;lod='Screen120_48 H20 controllable Actor LOD0';scope='fixed roster, flat authored field, UE shared RVO; not full match economy'}
$m.functional=[bool]$Functional
$m.runner_hashes=@(Get-FileHash $PSCommandPath,(Join-Path $PSScriptRoot 'Measure-LeaderPhase3.ps1') | Select-Object Path,Hash)
$m.source_hashes=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Source') -Recurse -File | Get-FileHash | Select-Object Path,Hash)
$m.config_hashes=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Config') -Filter '*.ini' | Get-FileHash | Select-Object Path,Hash)
$m.map_hash=(Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')).Hash
$m|ConvertTo-Json -Depth 9|Set-Content (Join-Path $out 'manifest.json')
$proc=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $args -WindowStyle Hidden -PassThru
if(!$proc.WaitForExit([int](($Warmup+$Duration+180)*1000))) { Stop-Process -Id $proc.Id; $m.status='timeout' } else {
 $text=Get-Content $log -Raw
 $ok=$proc.ExitCode -eq 0 -and $text -match 'P3_RESULT failures=0' -and $text -notmatch 'Fatal error:|Assertion failed:|Ensure condition failed:' -and (Test-Path (Join-Path $out 'frames.csv'))
 $m.status=if($ok){'passed'}else{'failed'}
 $m.exit_code=$proc.ExitCode
 $m.result=($text -split "`n" | Where-Object {$_ -match 'P3_RESULT'})
 $m.adapter=($text -split "`n" | Where-Object {$_ -match 'Chosen D3D12 Adapter'})
}
$m.completed=Get-Date -Format o
$m|ConvertTo-Json -Depth 9|Set-Content (Join-Path $out 'manifest.json')
Write-Output "$Units $Decision $View $Layout seed=$Seed : $($m.status)"
if($m.status -ne 'passed') { throw "Phase3 run failed: $log" }

} finally { $lock.Dispose() }
