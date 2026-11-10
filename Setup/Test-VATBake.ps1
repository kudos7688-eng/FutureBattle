param([Parameter(Mandatory=$true)][string]$Output, [switch]$Materials,[switch]$Validate,[switch]$RepairMetadata)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$project=Join-Path $repo 'Game/FutureBattle/FutureBattle.uproject'
$outPath=[IO.Path]::GetFullPath((Join-Path $repo $Output))
if(Test-Path (Join-Path $outPath 'manifest.json')) { throw 'Existing evidence folder' }
New-Item -ItemType Directory -Path $outPath -Force | Out-Null
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
@{machine=(Get-MeasurementMachine);purpose='Offline VAT asset generation, not performance';materials=[bool]$Materials;dll=(Get-FileHash (Join-Path $repo 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')).Hash} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $outPath 'manifest.json')
$extra=if($RepairMetadata){'-RepairMetadata'}elseif($Validate){'-Validate'}elseif($Materials){'-Materials'}else{''}
$proc=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList "`"$project`" -run=BattleVATBake $extra -unattended -nosound -AllowCommandletRendering -abslog=`"$outPath/run.log`"" -PassThru -WindowStyle Hidden
$proc.WaitForExit()
"exit=$($proc.ExitCode)" | Set-Content (Join-Path $outPath 'exit.txt')
if($proc.ExitCode -ne 0){throw "VAT commandlet failed: $($proc.ExitCode)"}
