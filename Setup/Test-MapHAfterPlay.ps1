$ErrorActionPreference = 'Stop'
$projectDir = (Resolve-Path (Join-Path $PSScriptRoot '../Game/FutureBattle')).Path
$runs = Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
[IO.Directory]::CreateDirectory($runs) | Out-Null

function Get-TestInputs {
    $files = @(
        (Join-Path $projectDir 'Content/Prototype/Lvl_BattleWide_H.umap')
        (Join-Path $projectDir 'Binaries/Win64/UnrealEditor-FutureBattle.dll')
        (Join-Path $projectDir 'FutureBattle.uproject')
        (Join-Path $PSScriptRoot 'Test-WideSight.ps1')
        $PSCommandPath
    ) + @(Get-ChildItem (Join-Path $projectDir 'Config') -Recurse -File | Sort-Object FullName | ForEach-Object FullName)
    foreach ($file in $files) { "$file=$((Get-FileHash -LiteralPath $file).Hash)" }
}

try {
    $lock = [IO.File]::Open((Join-Path $runs 'mapH-after-play.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
} catch [IO.IOException] {
    if (($_.Exception.HResult -band 0xffff) -ne 32) { throw }
    Write-Host 'Map H post-play test already running.'
    return
}
try {
    $inputs = (Get-TestInputs) -join "`n"
    foreach ($previous in Get-ChildItem $runs -Directory -Filter 'mapH-after-play-*') {
        $stamp = Join-Path $previous.FullName 'verified-inputs.txt'
        if ((Test-Path -LiteralPath $stamp) -and [IO.File]::ReadAllText($stamp) -eq $inputs -and
            (Test-Path (Join-Path $previous.FullName 'metrics.json')) -and (Test-Path (Join-Path $previous.FullName 'run.log'))) {
            Write-Host "Map H already verified: $($previous.FullName)"
            return
        }
    }
    $name = 'mapH-after-play-' + [Guid]::NewGuid().ToString('N')
    & (Join-Path $PSScriptRoot 'Test-WideSight.ps1') -Map H -Area -Experiment -Seed 7 -Fps 30 -CoverActions 1 -FlankCoordination 1 -AreaPointSupply 1 -OutputName $name
    $output = Join-Path $runs $name
    if (!(Get-Content (Join-Path $output 'result.json') -Raw | ConvertFrom-Json).passed -or
        !(Test-Path (Join-Path $output 'metrics.json'))) { throw "Incomplete Map H test: $output" }
    if (((Get-TestInputs) -join "`n") -eq $inputs) {
        [IO.File]::WriteAllText((Join-Path $output 'verified-inputs.txt'), $inputs)
    }
} finally {
    $lock.Dispose()
}
