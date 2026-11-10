[CmdletBinding()]
param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'Output/UncommittedInventory'))
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    # -z preserves spaces and Unicode paths; no staging or working-tree mutation.
    $raw = (& git -c core.quotepath=false status --porcelain=v1 -z --untracked-files=all) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'git status failed' }
    $entries = $raw.Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)
    $rows = for ($i = 0; $i -lt $entries.Length; $i++) {
        $entry = $entries[$i]
        $state = $entry.Substring(0, 2)
        $path = $entry.Substring(3)
        $original = ''
        if ($state -match '[RC]') { $original = $entries[++$i] }
        $group = switch -Regex ($path) {
            '^Setup/Benchmarks/' { 'measurement-evidence'; break }
            '^Setup/(MatchRuns|NetworkRuns|ScaleRuns|CPUProfiles)/' { 'regression-evidence'; break }
            '^Game/.*/(Source|Config)/|\.uproject$' { 'runtime-and-config'; break }
            '^Game/.*/Content/|^ThirdParty/' { 'assets-and-licenses'; break }
            '^Setup/.*\.(ps1|py|cpp)$|^Play-.*\.cmd$' { 'tools-and-launchers'; break }
            '\.md$|^\.gitignore$' { 'documentation-and-policy'; break }
            default { 'review-local-output' }
        }
        $item = Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
        [pscustomobject]@{Status=$state; Group=$group; Path=$path; OriginalPath=$original; Bytes=if ($item) {$item.Length} else {0}}
    }
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $rows | Sort-Object Group,Path | Export-Csv -LiteralPath (Join-Path $OutputDirectory 'files.csv') -NoTypeInformation -Encoding utf8
    $summary = @($rows | Group-Object Group | ForEach-Object {
        [pscustomobject]@{Group=$_.Name; Files=$_.Count; Bytes=($_.Group | Measure-Object Bytes -Sum).Sum}
    })
    $summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding utf8
    $summary | Format-Table -AutoSize
    Write-Output "Inventory: $OutputDirectory"
} finally { Pop-Location }
