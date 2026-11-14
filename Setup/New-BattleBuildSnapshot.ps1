[CmdletBinding()]
param([Parameter(Mandatory)][string]$Name)
$ErrorActionPreference='Stop'
if($Name -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple snapshot name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project=Join-Path $root 'Game/FutureBattle'
$out=Join-Path $PSScriptRoot "Benchmarks/BuildSnapshots/$Name"
if(Test-Path $out){throw "Snapshot exists: $out"}
$dll=Join-Path $project 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$binary=Get-Item $dll
$sources=@(Get-ChildItem (Join-Path $project 'Source') -Recurse -File)
if(@($sources|Where-Object{$_.LastWriteTimeUtc -gt $binary.LastWriteTimeUtc}).Count){throw 'Build current sources before snapshotting'}
$files=@($sources)+@(Get-ChildItem (Join-Path $project 'Config') -Recurse -File)+@(Get-Item (Join-Path $project 'FutureBattle.uproject'))
$hashes=@($files|ForEach-Object{@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
$dllHash=(Get-FileHash $dll).Hash
New-Item -ItemType Directory -Path $out|Out-Null
$archive=Join-Path $out 'source.zip'
Compress-Archive -Path (Join-Path $project 'Source'),(Join-Path $project 'Config'),(Join-Path $project 'FutureBattle.uproject') -DestinationPath $archive
foreach($file in $hashes){if((Get-FileHash $file.path).Hash -ne $file.sha256){throw 'Source changed during snapshot'}}
if((Get-FileHash $dll).Hash -ne $dllHash){throw 'DLL changed during snapshot'}
$maps=@(Get-ChildItem (Join-Path $project 'Content/Prototype') -Filter 'Lvl_Battle*.umap' -File|ForEach-Object{@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
@{created_at_utc=[DateTime]::UtcNow.ToString('o');dll=$dllHash;files=$hashes;maps=$maps;sourceArchive=$archive;sourceArchiveSha256=(Get-FileHash $archive).Hash}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
Write-Output (Join-Path $out 'manifest.json')
