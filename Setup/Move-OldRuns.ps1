# Move old run stdout logs / screenshots (*.txt, *.png, *.json) that are untracked by git out of Setup/ into
# Setup/Runs/<yyyy-MM>/ (git-ignored). Tracked files, scripts and anything newer than -Days are left alone.
# Nothing in Setup reads these files back; match results live in Benchmarks/. See MEASURE.md.
[CmdletBinding()]
param([ValidateRange(1,365)][int]$Days=2,[switch]$WhatIf)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$untracked=@{}
git ls-files --others --exclude-standard -- . | Where-Object { $_ -notmatch '/' } | ForEach-Object { $untracked[$_]=$true }
$limit=(Get-Date).AddDays(-$Days)
$moved=0
foreach($f in Get-ChildItem -File | Where-Object { $_.Extension -in '.txt','.png','.json' -and $_.LastWriteTime -lt $limit -and $untracked.ContainsKey($_.Name) }){
 $dest=Join-Path $PSScriptRoot ('Runs/'+$f.LastWriteTime.ToString('yyyy-MM'))
 if(Test-Path (Join-Path $dest $f.Name)){Write-Warning "skip (exists): $($f.Name)";continue}
 if($WhatIf){Write-Output "would move $($f.Name) -> $dest";$moved++;continue}
 New-Item -ItemType Directory -Force -Path $dest|Out-Null
 Move-Item -LiteralPath $f.FullName -Destination $dest
 $moved++
}
Write-Output "moved $moved file(s) older than $Days day(s)"
