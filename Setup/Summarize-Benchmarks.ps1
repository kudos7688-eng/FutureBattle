[CmdletBinding()]
param([Parameter(Mandatory)][string[]]$Reports)
$ErrorActionPreference = 'Stop'
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$rows = foreach ($path in $Reports) {
    foreach ($file in Get-Item -Path $path) {
        if (!$seen.Add($file.FullName)) { continue }
        $r = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if (!$r.binary_fingerprint -or !$r.source_fingerprint -or !$r.map_fingerprint) { throw "Report lacks implementation fingerprints; compare legacy reports manually: $($file.Name)" }
        if (!$r.frame_wall_ms -or $r.frames -le 0 -or $r.duration_seconds -lt $r.requested_seconds -or $r.counts.shots -le 0) { throw "Incomplete benchmark: $($file.Name)" }
        $key = [ordered]@{}
        foreach ($field in @('engine','unit_count','seed','layout','camera','validated_dense_goals','animation_lod','hidden_animation_culling','stable_load','staggered','rendered','perception_slices','initial_health','ally_training','enemy_training','csv_profile','diagnostic_omission','sense_mode','source_fingerprint','binary_fingerprint','map_fingerprint','requested_seconds','hardware')) { $key[$field] = $r.$field }
        [pscustomobject]@{ Key=($key | ConvertTo-Json -Depth 8 -Compress); Report=$r; File=$file.FullName }
    }
}
foreach ($group in $rows | Group-Object Key) {
    $runs = @($group.Group.Report)
    $sample = $runs[0]
    $means = @($runs | ForEach-Object { [double]$_.frame_wall_ms.mean } | Sort-Object)
    $middle = [int][Math]::Floor($means.Count / 2)
    $median = if ($means.Count % 2) { $means[$middle] } else { ($means[$middle - 1] + $means[$middle]) / 2 }
    [pscustomobject]@{
        Units=$sample.unit_count; Rendered=$sample.rendered; Seed=$sample.seed; Runs=$runs.Count
        SenseMode=$sample.sense_mode; CSVProfile=$sample.csv_profile; DiagnosticOmission=$sample.diagnostic_omission
        Binary=$sample.binary_fingerprint.Substring(0,12)
        MeanFrameMinMS=[Math]::Round($means[0],3); MeanFrameMedianMS=[Math]::Round($median,3); MeanFrameMaxMS=[Math]::Round($means[-1],3)
        WorstRunP95MS=[Math]::Round(($runs.frame_wall_ms.p95 | Measure-Object -Maximum).Maximum,3)
        WorstFrameMS=[Math]::Round(($runs.frame_wall_ms.max | Measure-Object -Maximum).Maximum,3)
        WorstAIMS=[Math]::Round(($runs.combined_ai_ms_per_frame.max | Measure-Object -Maximum).Maximum,3)
        TotalMoveFailures=($runs.counts.move_failures | Measure-Object -Sum).Sum
        Reports=@($group.Group.File)
    }
}
