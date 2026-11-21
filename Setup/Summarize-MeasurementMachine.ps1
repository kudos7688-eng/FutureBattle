#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$InputPath,
    [Parameter(Mandatory=$true)][string]$OutputPrefix,
    [ValidateRange(1,86400)][int]$IntervalSeconds=30,
    [ValidateRange(0.01,0.99)][double]$ClockDropRatio=0.7,
    [ValidateRange(0,10000)][double]$BaselinePerformancePercent=0
)
$ErrorActionPreference='Stop'
function Stats($values, [int]$expected) {
    $valid=@($values | Where-Object { $null -ne $_ })
    if (-not $valid.Count) { return @{count=0;missing=$expected;average=$null;maximum=$null;minimum=$null} }
    $s=$valid | Measure-Object -Average -Maximum -Minimum
    return @{count=$valid.Count;missing=($expected-$valid.Count);average=$s.Average;maximum=$s.Maximum;minimum=$s.Minimum}
}
function Flatten($row) {
    $flat=@{collection_seconds=$row.collection_seconds}
    foreach ($p in $row.metrics.PSObject.Properties) { $flat[$p.Name]=$p.Value }
    foreach ($t in $row.targets) {
        foreach ($field in @('cpu_delta_seconds','cpu_percent','private_bytes','working_set_bytes','thread_count')) { $flat[('target.{0}.{1}' -f $t.pid,$field)]=$t.$field }
    }
    foreach ($gpu in $row.gpu) {
        if ($null -eq $gpu) { continue }
        foreach ($field in @('graphics_mhz','sm_mhz','memory_mhz','power_w','temperature_c','utilization_percent','vram_used_mib','vram_total_mib')) { $flat[('gpu.{0}.{1}' -f $gpu.uuid,$field)]=$gpu.$field }
    }
    return $flat
}
$rows=New-Object 'Collections.Generic.List[object]'
$invalid=New-Object 'Collections.Generic.List[object]'
$lineNumber=0
Get-Content -LiteralPath $InputPath -Encoding UTF8 | ForEach-Object {
    $lineNumber++
    if ($_.Trim()) {
        try {
            $r=$_ | ConvertFrom-Json
            if ($r.schema_version -ne 1 -or $null -eq $r.elapsed_seconds -or -not $r.timestamp_utc -or $null -eq $r.metrics) { throw 'Unsupported or incomplete sample' }
            [void][DateTimeOffset]::Parse($r.timestamp_utc)
            $rows.Add($r)
        } catch { $invalid.Add(@{line=$lineNumber;reason=$_.Exception.Message}) }
    }
}
if (-not $rows.Count) { throw 'No valid samples in input' }
$ordered=@($rows | Sort-Object elapsed_seconds)
$performance=@($ordered | ForEach-Object { $_.metrics.processor_performance_percent } | Where-Object { $null -ne $_ -and $_ -gt 0 } | Sort-Object)
$baseline=$null; $baselineSource='unavailable (fewer than 5 valid positive samples)'
if ($BaselinePerformancePercent -gt 0) { $baseline=$BaselinePerformancePercent; $baselineSource='explicit known-normal baseline' }
elseif ($performance.Count -ge 5) { $baseline=$performance[[math]::Ceiling($performance.Count*0.9)-1]; $baselineSource='within-run p90; heuristic, not a verified normal baseline' }
$bins=@{}
foreach ($r in $ordered) {
    $bin=[int][math]::Floor($r.elapsed_seconds/$IntervalSeconds)
    if (-not $bins.ContainsKey($bin)) { $bins[$bin]=New-Object 'Collections.Generic.List[object]' }
    $bins[$bin].Add($r)
}
$intervals=@(foreach ($bin in @($bins.Keys | Sort-Object)) {
    $samples=@($bins[$bin].ToArray())
    $flats=@($samples | ForEach-Object { Flatten $_ })
    $keys=@($flats | ForEach-Object { $_.Keys } | Sort-Object -Unique)
    $metrics=[ordered]@{}
    foreach ($key in $keys) { $metrics[$key]=Stats @($flats | ForEach-Object { $_[$key] }) $samples.Count }
    $others=@{}
    foreach ($r in $samples) {
        foreach ($p in $r.top_processes) {
            if ($p.is_target -or $p.pid -in $r.target_ids) { continue }
            $key='{0}/{1}' -f $p.pid,$p.start_utc
            if (-not $others.ContainsKey($key)) { $others[$key]=@{name=$p.name;pid=$p.pid;start_utc=$p.start_utc;values=New-Object 'Collections.Generic.List[double]';cpu_seconds=0.0} }
            $others[$key].values.Add([double]$p.cpu_percent)
            $others[$key].cpu_seconds+=$p.cpu_percent/100*$r.logical_processors*$r.interval_seconds
        }
    }
    $otherSummary=@(foreach ($p in $others.Values) {
        [pscustomobject]@{name=$p.name;pid=$p.pid;start_utc=$p.start_utc;top10_appearances=$p.values.Count;observed_cpu_seconds=$p.cpu_seconds;cpu_percent_when_in_top10=(Stats $p.values.ToArray() $p.values.Count)}
    })
    $low=@($samples | Where-Object { $null -ne $baseline -and $null -ne $_.metrics.processor_performance_percent -and $_.metrics.processor_performance_percent -lt $baseline*$ClockDropRatio })
    $reasons=@($samples | ForEach-Object { $_.reasons.PSObject.Properties | ForEach-Object { '{0}: {1}' -f $_.Name,$_.Value } } | Sort-Object -Unique)
    [ordered]@{
        start_seconds=$bin*$IntervalSeconds; end_seconds=($bin+1)*$IntervalSeconds
        first_timestamp_utc=$samples[0].timestamp_utc; last_timestamp_utc=$samples[-1].timestamp_utc; samples=$samples.Count
        metrics=$metrics; other_top_processes=@($otherSummary | Sort-Object observed_cpu_seconds -Descending | Select-Object -First 10)
        clock_low_samples=$low.Count; clock_valid_samples=@($samples | Where-Object {$null -ne $_.metrics.processor_performance_percent}).Count
        clock_low_timestamps_utc=@($low | ForEach-Object {$_.timestamp_utc})
        foreground_pids=@($samples.foreground_pid | Where-Object {$null -ne $_} | Sort-Object -Unique)
        power_plans=@($samples | ForEach-Object { if ($_.power_plan) { '{0} ({1})' -f $_.power_plan.guid,$_.power_plan.name } } | Sort-Object -Unique)
        minimized_target_samples=@($samples | ForEach-Object {$_.targets} | Where-Object {$_.minimized -eq $true}).Count
        reasons=$reasons
    }
})
$summary=[ordered]@{
    schema_version=1; input_path=[IO.Path]::GetFullPath($InputPath); sample_count=$ordered.Count
    invalid_lines=$invalid.ToArray(); interval_seconds=$IntervalSeconds
    baseline_performance_percent=$baseline; baseline_source=$baselineSource; clock_drop_ratio=$ClockDropRatio
    averaging='Arithmetic mean of available samples; null excluded, missing counts retained. Intervals use sample endpoint elapsed_seconds.'
    limitations=@('Other-process CPU is only observed while in top 10; absent samples are unknown, not zero.', 'Clock drops can reflect idle or workload changes; they do not prove thermal/power throttling.', 'Page Reads/sec is system hard-fault servicing disk reads, not per-process fault count; one read can serve multiple pages.')
    intervals=$intervals
}
$lines=New-Object 'Collections.Generic.List[string]'
$lines.Add(('Samples: {0}; invalid lines: {1}; interval: {2}s' -f $ordered.Count,$invalid.Count,$IntervalSeconds))
$lines.Add(('Clock baseline (% Processor Performance): {0}; {1}; low threshold: baseline x {2}' -f $baseline,$baselineSource,$ClockDropRatio))
foreach ($i in $intervals) {
    $cpu=$i.metrics['cpu_percent']; $perf=$i.metrics['processor_performance_percent']
    $other=@($i.other_top_processes | Select-Object -First 3 | ForEach-Object { '{0}[{1}] {2:N1}s CPU' -f $_.name,$_.pid,$_.observed_cpu_seconds }) -join ', '
    $lines.Add(('[{0}-{1}s] CPU avg/max={2:N1}/{3:N1}%; performance avg={4:N1}%; clock low={5}/{6}; other top: {7}' -f $i.start_seconds,$i.end_seconds,$cpu.average,$cpu.maximum,$perf.average,$i.clock_low_samples,$i.clock_valid_samples,$other))
}
$lines.Add('Means exclude missing data. Other CPU is top-10-only. Clock flags are candidates, not a thermal/power diagnosis. See JSON for memory, disk, target and GPU statistics.')
$prefix=[IO.Path]::GetFullPath($OutputPrefix)
if ($prefix+'.json' -eq [IO.Path]::GetFullPath($InputPath) -or $prefix+'.txt' -eq [IO.Path]::GetFullPath($InputPath)) { throw 'Output must not overwrite input' }
if ((Test-Path -LiteralPath ($prefix+'.json')) -or (Test-Path -LiteralPath ($prefix+'.txt'))) { throw 'Summary output already exists; choose a new OutputPrefix' }
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($prefix))
$summary | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath ($prefix+'.json') -Encoding UTF8
$lines | Set-Content -LiteralPath ($prefix+'.txt') -Encoding UTF8
$lines
