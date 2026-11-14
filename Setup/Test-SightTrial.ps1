[CmdletBinding()]
param([ValidateRange(0,16000)][int]$SightRange=0,[ValidateSet(-1,0,1)][int]$RangeAware=-1,[string]$OutputName='baseline',[switch]$Rendered,[ValidateRange(0,10000)][int]$WeaponRange=0,[switch]$WeaponProbe)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output identifier required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$out=Join-Path $PSScriptRoot ('Benchmarks/SightTrial-20261111/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out | Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$files=Get-ChildItem (Join-Path $root 'Game/FutureBattle/Source'),(Join-Path $root 'Game/FutureBattle/Config') -Recurse -File
$hashes=@($files | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
$map=Join-Path $root 'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap'
$log=Join-Path $out 'run.log'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1','-BattleWideMixed',('-abslog="'+$log+'"'))
$arguments+=if($WeaponProbe){'-BattleWeaponTrial'}else{'-BattleSightTrial'}
if($WeaponRange){$arguments+="-BattleWeaponRange=$WeaponRange"}
if($SightRange){$arguments+="-BattleSightRange=$SightRange"}
if($RangeAware -ge 0){$arguments+="-BattleRangeAware=$RangeAware"}
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080')}else{$arguments+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;files=$hashes;map=(Get-FileHash $map).Hash;arguments=$arguments} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(150000)){Stop-Process -Id $p.Id;throw 'Sight trial timeout'}
$content=Get-Content $log -Raw
$expected=if($WeaponProbe){'BATTLE_WEAPON_RESULT failures=0 cases=12'}else{'BATTLE_SIGHT_RESULT failures=0 cases=8'}
$passed=$p.ExitCode -eq 0 -and $content -match $expected -and $content -notmatch 'Fatal error:|Assertion failed:'
@{passed=$passed;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash;cases=@($content -split "`n" | Where-Object {$_ -match 'BATTLE_(SIGHT|WEAPON)_CASE'})} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$content -split "`n" | Where-Object {$_ -match 'BATTLE_(SIGHT|WEAPON)_(OVERRIDE|CASE|RESULT)'} | Write-Output
if(!$passed){throw "Sight trial failed: $log"}
