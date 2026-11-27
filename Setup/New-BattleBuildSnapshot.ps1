[CmdletBinding()]
param([Parameter(Mandatory)][string]$Name,[switch]$NoProjectCopy)
# Since 2026-11-18 a snapshot holds its own copy of the runnable project (DLL, .modules, Config, Content, .uproject,
# Saved/Config, DerivedDataCache) under <snapshot>/Project, and Test-WideSight.ps1 starts UE from that copy. Builds and
# Config/map edits in the working tree then no longer stop or fail a running matrix. About 200 MB per snapshot; delete old
# snapshot folders when space is short. -NoProjectCopy keeps the old hash-only snapshot (runs need the live tree unchanged).
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
$copy=$null
if(!$NoProjectCopy){
 $copy=Join-Path $out 'Project'
 New-Item -ItemType Directory -Path (Join-Path $copy 'Binaries/Win64'),(Join-Path $copy 'Saved') -Force|Out-Null
 foreach($f in 'UnrealEditor-FutureBattle.dll','UnrealEditor.modules'){Copy-Item (Join-Path $project "Binaries/Win64/$f") (Join-Path $copy 'Binaries/Win64')}
 Copy-Item (Join-Path $project 'FutureBattle.uproject') $copy
 foreach($d in 'Config','DerivedDataCache'){if(Test-Path (Join-Path $project $d)){Copy-Item (Join-Path $project $d) $copy -Recurse}}
 # Content (about 660 MB, nearly the same in every snapshot) is not copied again when the newest earlier snapshot holds the
 # same file: a file of 64 KB or more with the same relative path, size and SHA-256 becomes a hard link to that frozen
 # copy (never to the live tree). Deleting one snapshot's Project leaves the others intact. Since 2026-11-27 (disk full).
 $content=Join-Path $project 'Content'
 if(Test-Path $content){
  $prev=@(Get-ChildItem (Join-Path $PSScriptRoot 'Benchmarks/BuildSnapshots') -Directory|Where-Object{$_.FullName -ne $out -and (Test-Path (Join-Path $_.FullName 'Project/Content')) -and (Test-Path (Join-Path $_.FullName 'manifest.json'))}|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 1)
  $prevContent=if($prev.Count){Join-Path $prev[0].FullName 'Project/Content'}else{$null}
  $linked=0;$copied=0
  foreach($f in Get-ChildItem $content -Recurse -File){
   $rel=$f.FullName.Substring($content.Length).TrimStart('\','/')
   $dest=Join-Path (Join-Path $copy 'Content') $rel
   New-Item -ItemType Directory -Path (Split-Path $dest) -Force|Out-Null
   $old=if($prevContent){Join-Path $prevContent $rel}else{$null}
   $done=$false
   if($old -and $f.Length -ge 65536 -and (Test-Path -LiteralPath $old) -and (Get-Item -LiteralPath $old).Length -eq $f.Length -and (Get-FileHash -LiteralPath $old).Hash -eq (Get-FileHash -LiteralPath $f.FullName).Hash){
    try{New-Item -ItemType HardLink -Path $dest -Target $old -ErrorAction Stop|Out-Null;$done=$true;$linked++}catch{$done=$false}
   }
   if(!$done){Copy-Item -LiteralPath $f.FullName -Destination $dest;$copied++}
  }
  Write-Verbose "Content: $linked linked to $($prev.Name), $copied copied"
 }
 if(Test-Path (Join-Path $project 'Saved/Config')){Copy-Item (Join-Path $project 'Saved/Config') (Join-Path $copy 'Saved') -Recurse}
 if((Get-FileHash (Join-Path $copy 'Binaries/Win64/UnrealEditor-FutureBattle.dll')).Hash -ne $dllHash){throw 'DLL changed during snapshot'}
}
foreach($file in $hashes){if((Get-FileHash $file.path).Hash -ne $file.sha256){throw 'Source changed during snapshot'}}
if((Get-FileHash $dll).Hash -ne $dllHash){throw 'DLL changed during snapshot'}
$runRoot=if($copy){$copy}else{$project}
# With a copy, the checked runtime files (Config, .uproject, maps) are the copied ones; Source entries stay as a record.
if($copy){$hashes=@($hashes|Where-Object{$_.path -match '[\\/]Source[\\/]'})+@(Get-ChildItem (Join-Path $copy 'Config') -Recurse -File|ForEach-Object{@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})+@(@{path=(Join-Path $copy 'FutureBattle.uproject');sha256=(Get-FileHash (Join-Path $copy 'FutureBattle.uproject')).Hash})}
$maps=@(Get-ChildItem (Join-Path $runRoot 'Content/Prototype') -Filter 'Lvl_Battle*.umap' -File|ForEach-Object{@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
@{created_at_utc=[DateTime]::UtcNow.ToString('o');dll=$dllHash;files=$hashes;maps=$maps;sourceArchive=$archive;sourceArchiveSha256=(Get-FileHash $archive).Hash;projectCopy=$copy}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
Write-Output (Join-Path $out 'manifest.json')
