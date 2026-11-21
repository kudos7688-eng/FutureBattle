#requires -Version 5.1
[CmdletBinding(DefaultParameterSetName='Name')]
param(
    [Parameter(Mandatory=$true, ParameterSetName='Id')][ValidateRange(1,2147483647)][int[]]$TargetId,
    [Parameter(ParameterSetName='Name')][string]$TargetName = 'UnrealEditor*',
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [ValidateRange(0,864000)][int]$DurationSeconds = 0
)
$ErrorActionPreference = 'Stop'
# A persistent PDH query collects all counters together, without localized names
# or the extra one-second warmup of a new Get-Counter query on each iteration.
if (-not ('MeasurementWatch.Native' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;
namespace MeasurementWatch {
 public sealed class Native : IDisposable {
  [DllImport("pdh.dll", CharSet=CharSet.Unicode)] static extern uint PdhOpenQuery(string s, IntPtr d, out IntPtr q);
  [DllImport("pdh.dll", CharSet=CharSet.Unicode)] static extern uint PdhAddEnglishCounter(IntPtr q,string p,IntPtr d,out IntPtr c);
  [DllImport("pdh.dll")] static extern uint PdhCollectQueryData(IntPtr q);
  [StructLayout(LayoutKind.Explicit)] struct Value { [FieldOffset(0)] public uint status; [FieldOffset(8)] public double number; }
  [DllImport("pdh.dll")] static extern uint PdhGetFormattedCounterValue(IntPtr c,uint f,out uint t,out Value v);
  [DllImport("pdh.dll")] static extern uint PdhCloseQuery(IntPtr q);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr w);
  [DllImport("powrprof.dll")] static extern uint PowerGetActiveScheme(IntPtr root,out IntPtr scheme);
  [DllImport("powrprof.dll", CharSet=CharSet.Unicode)] static extern uint PowerReadFriendlyName(IntPtr root,ref Guid scheme,IntPtr sub,IntPtr setting,byte[] buffer,ref uint size);
  [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
  IntPtr query; Dictionary<string,IntPtr> counters = new Dictionary<string,IntPtr>();
  public Native() { uint s=PdhOpenQuery(null,IntPtr.Zero,out query); if(s!=0) throw new Exception("PdhOpenQuery: "+s); }
  public uint Add(string key,string path) { IntPtr c; uint s=PdhAddEnglishCounter(query,path,IntPtr.Zero,out c); if(s==0)counters[key]=c; return s; }
  public uint Collect() { return PdhCollectQueryData(query); }
  public double Read(string key) { if(!counters.ContainsKey(key))throw new Exception("counter unavailable"); uint t; Value v; uint s=PdhGetFormattedCounterValue(counters[key],0x200|0x8000,out t,out v); if(s!=0 || v.status>1)throw new Exception("PDH status: "+s+"/"+v.status); return v.number; }
  public static string[] Power() { IntPtr p; uint s=PowerGetActiveScheme(IntPtr.Zero,out p); if(s!=0)throw new Exception("PowerGetActiveScheme: "+s); try { Guid g=(Guid)Marshal.PtrToStructure(p,typeof(Guid)); uint n=0; PowerReadFriendlyName(IntPtr.Zero,ref g,IntPtr.Zero,IntPtr.Zero,null,ref n); byte[] b=new byte[n]; s=PowerReadFriendlyName(IntPtr.Zero,ref g,IntPtr.Zero,IntPtr.Zero,b,ref n); return new string[]{g.ToString(),s==0?Encoding.Unicode.GetString(b).TrimEnd('\0'):null}; } finally {LocalFree(p);} }
  public void Dispose() { if(query!=IntPtr.Zero) {PdhCloseQuery(query);query=IntPtr.Zero;} }
 }
}
'@
}
function Snapshot {
    $result = @{}
    foreach ($proc in @(Get-Process)) {
        $entry = [ordered]@{ name=$proc.ProcessName; pid=$proc.Id; start=$null; cpu=$null; private_bytes=$null; working_set_bytes=$null; threads=$null; minimized=$null; reasons=@{} }
        foreach ($field in @('start','cpu','private_bytes','working_set_bytes','threads','minimized')) {
            try {
                $entry[$field] = switch ($field) {
                    start { $proc.StartTime.ToUniversalTime().ToString('o') }
                    cpu { $proc.TotalProcessorTime.TotalSeconds }
                    private_bytes { $proc.PrivateMemorySize64 }
                    working_set_bytes { $proc.WorkingSet64 }
                    threads { if ($script:tracked.ContainsKey($proc.Id)) { $proc.Threads.Count } else { $null } }
                    minimized {
                        if ($script:tracked.ContainsKey($proc.Id)) {
                            if ($proc.MainWindowHandle -eq [IntPtr]::Zero) { throw 'No main window (headless, hidden, or not yet created)' }
                            [MeasurementWatch.Native]::IsIconic($proc.MainWindowHandle)
                        } else { $null }
                    }
                }
            } catch { $entry.reasons[$field] = $_.Exception.Message }
        }
        $result[$proc.Id] = $entry
        $proc.Dispose()
    }
    return $result
}
function Read-Gpu {
    if (-not $script:smi) { return @{ value=$null; reason='nvidia-smi not found on PATH or standard NVSMI path' } }
    $p = New-Object System.Diagnostics.Process
    try {
        $p.StartInfo.FileName = $script:smi
        $p.StartInfo.Arguments = '--query-gpu=uuid,name,clocks.current.graphics,clocks.current.sm,clocks.current.memory,pstate,power.draw,temperature.gpu,utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits'
        $p.StartInfo.UseShellExecute = $false
        $p.StartInfo.CreateNoWindow = $true
        $p.StartInfo.RedirectStandardOutput = $true
        $p.StartInfo.RedirectStandardError = $true
        [void]$p.Start()
        $outTask = $p.StandardOutput.ReadToEndAsync()
        $errTask = $p.StandardError.ReadToEndAsync()
        if (-not $p.WaitForExit(600)) { $p.Kill(); throw 'nvidia-smi timeout (600 ms)' }
        if ($p.ExitCode -ne 0) { throw ('nvidia-smi: ' + $errTask.Result) }
        $items = @(foreach ($line in ($outTask.Result -split '\r?\n')) {
            if (-not $line.Trim()) { continue }
            $fields = @('uuid','name','graphics_mhz','sm_mhz','memory_mhz','pstate','power_w','temperature_c','utilization_percent','vram_used_mib','vram_total_mib')
            $csv = $line | ConvertFrom-Csv -Header $fields
            $item = [ordered]@{ reasons=@{} }
            foreach ($field in $fields) {
                $v = ([string]$csv.$field).Trim()
                $number = 0.0
                if ($field -in @('uuid','name','pstate') -and $v -and $v -notmatch 'N/A|Not Supported') { $item[$field]=$v }
                elseif ([double]::TryParse($v,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number)) { $item[$field]=$number }
                else { $item[$field]=$null; $item.reasons[$field]="Unavailable: $v" }
            }
            $item
        })
        if (-not $items.Count) { throw 'nvidia-smi returned no GPUs' }
        return @{ value=$items; reason=$null }
    } catch { return @{ value=$null; reason=$_.Exception.Message } }
    finally { $p.Dispose() }
}

$tracked = @{}
$selected = if ($PSCmdlet.ParameterSetName -eq 'Id') { @(Get-Process -Id $TargetId -ErrorAction SilentlyContinue) } else { @(Get-Process -Name $TargetName -ErrorAction SilentlyContinue) }
foreach ($proc in $selected) { $tracked[$proc.Id] = $proc; }
if (-not $tracked.Count) { throw 'No matching target process is running. Start the target first.' }
$logical = [Environment]::ProcessorCount
$smi = (Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $smi -and (Test-Path "$env:ProgramFiles\NVIDIA Corporation\NVSMI\nvidia-smi.exe")) { $smi="$env:ProgramFiles\NVIDIA Corporation\NVSMI\nvidia-smi.exe" }
$paths = [ordered]@{
    cpu_percent='\Processor(_Total)\% Processor Time'
    available_bytes='\Memory\Available Bytes'
    committed_bytes='\Memory\Committed Bytes'
    commit_limit_bytes='\Memory\Commit Limit'
    hard_faults_per_second='\Memory\Page Reads/sec'
    pages_input_per_second='\Memory\Pages Input/sec'
    disk_read_wait_seconds='\PhysicalDisk(_Total)\Avg. Disk sec/Read'
    disk_write_wait_seconds='\PhysicalDisk(_Total)\Avg. Disk sec/Write'
    disk_queue_length='\PhysicalDisk(_Total)\Current Disk Queue Length'
    processor_performance_percent='\Processor Information(_Total)\% Processor Performance'
    processor_frequency_mhz='\Processor Information(_Total)\Processor Frequency'
}
$full = [IO.Path]::GetFullPath($OutputPath)
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($full))
$stream=$null; $writer=$null; $query=$null
try {
    $stream = New-Object IO.FileStream($full,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    $writer = New-Object IO.StreamWriter($stream,(New-Object Text.UTF8Encoding($false)))
    $writer.AutoFlush=$true
    $counterErrors=@{}
    try {
        $query=New-Object MeasurementWatch.Native
        foreach ($key in $paths.Keys) { $status=$query.Add($key,$paths[$key]); if ($status -ne 0) { $counterErrors[$key]="PDH add failed: $status" } }
        [void]$query.Collect()
    } catch { foreach ($key in $paths.Keys) { $counterErrors[$key]=$_.Exception.Message } }
    $previous=Snapshot
    $clock=[Diagnostics.Stopwatch]::StartNew()
    $previousTime=0.0; $next=1.0
    while ($true) {
        $delay=$next-$clock.Elapsed.TotalSeconds
        if ($delay -gt 0) { Start-Sleep -Milliseconds ([int]($delay*1000)) }
        $alive=@(foreach ($id in @($tracked.Keys)) { try { if (-not $tracked[$id].HasExited) { $id } } catch {} })
        if (-not $alive.Count) { break }
        $sampleStart=$clock.Elapsed.TotalSeconds
        $current=Snapshot
        $sampleTime=$clock.Elapsed.TotalSeconds
        $elapsed=$sampleTime-$previousTime
        $row=[ordered]@{ schema_version=1; timestamp_utc=[DateTime]::UtcNow.ToString('o'); elapsed_seconds=$sampleTime; interval_seconds=$elapsed; logical_processors=$logical; target_ids=@($tracked.Keys); metrics=[ordered]@{}; top_processes=@(); targets=@(); power_plan=$null; foreground_pid=$null; gpu=$null; reasons=@{}; collection_seconds=$null }
        $collectStatus=$null
        if ($query) { $collectStatus=$query.Collect() }
        foreach ($key in $paths.Keys) {
            $row.metrics[$key]=$null
            try {
                if ($counterErrors.ContainsKey($key)) { throw $counterErrors[$key] }
                if ($collectStatus -ne 0) { throw "PDH collect failed: $collectStatus" }
                $row.metrics[$key]=$query.Read($key)
            } catch { $row.reasons[$key]=$_.Exception.Message }
        }
        $ranked=@(foreach ($id in $current.Keys) {
            $entry=$current[$id]; $delta=$null; $percent=$null
            if ($previous.ContainsKey($id) -and $null -ne $entry.start -and $null -ne $entry.cpu -and $null -ne $previous[$id].cpu -and $entry.start -eq $previous[$id].start) {
                $delta=$entry.cpu-$previous[$id].cpu
                if ($delta -ge 0) { $percent=100*$delta/$elapsed/$logical } else { $delta=$null }
            }
            if ($null -eq $delta) { $entry.reasons['cpu_delta_seconds']='No comparable previous CPU sample (new, exited, inaccessible, or reused PID)' }
            if ($id -in $alive) { $row.targets+= [ordered]@{ name=$entry.name; pid=$id; start_utc=$entry.start; cpu_delta_seconds=$delta; cpu_percent=$percent; private_bytes=$entry.private_bytes; working_set_bytes=$entry.working_set_bytes; thread_count=$entry.threads; minimized=$entry.minimized; reasons=$entry.reasons } }
            if ($id -ne 0 -and $null -ne $percent) { [pscustomobject]@{name=$entry.name;pid=$id;start_utc=$entry.start;cpu_percent=$percent;working_set_bytes=$entry.working_set_bytes;is_target=$tracked.ContainsKey($id);reasons=$entry.reasons} }
        })
        $row.top_processes=@($ranked | Sort-Object cpu_percent -Descending | Select-Object -First 10)
        foreach ($id in $alive) {
            if (-not $current.ContainsKey($id)) {
                $row.targets+=@{pid=$id;name=$null;start_utc=$null;cpu_delta_seconds=$null;cpu_percent=$null;private_bytes=$null;working_set_bytes=$null;thread_count=$null;minimized=$null;reasons=@{all_fields='Target exited or became inaccessible during snapshot'}}
            }
        }
        $row.reasons['process_coverage']='Top 10 excludes Idle and processes without comparable CPU samples; processes exiting between snapshots may be missed.'
        try { $plan=[MeasurementWatch.Native]::Power(); $row.power_plan=@{guid=$plan[0];name=$plan[1]}; if (-not $plan[1]) { $row.reasons['power_plan.name']='PowerReadFriendlyName unavailable' } } catch { $row.reasons['power_plan']=$_.Exception.Message }
        try { $window=[MeasurementWatch.Native]::GetForegroundWindow(); if ($window -eq [IntPtr]::Zero) { throw 'No foreground window in this session' }; [uint32]$front=0; [void][MeasurementWatch.Native]::GetWindowThreadProcessId($window,[ref]$front); $row.foreground_pid=$front } catch { $row.reasons['foreground_pid']=$_.Exception.Message }
        $gpu=Read-Gpu; $row.gpu=$gpu.value; if ($gpu.reason) { $row.reasons['gpu']=$gpu.reason }
        $row.collection_seconds=$clock.Elapsed.TotalSeconds-$sampleStart
        $writer.WriteLine(($row | ConvertTo-Json -Depth 10 -Compress))
        $previous=$current; $previousTime=$sampleTime
        if ($DurationSeconds -gt 0 -and $clock.Elapsed.TotalSeconds -ge $DurationSeconds) { break }
        $next+=1.0
        if ($next -lt $clock.Elapsed.TotalSeconds) { $next=$clock.Elapsed.TotalSeconds+1.0 }
    }
} finally {
    if ($query) { $query.Dispose() }
    if ($writer) { $writer.Dispose() } elseif ($stream) { $stream.Dispose() }
    foreach ($proc in $tracked.Values) { $proc.Dispose() }
}
