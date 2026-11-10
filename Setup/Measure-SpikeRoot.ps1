# Diagnostic recordings only. No C++/configuration/asset changes or affinity changes.
[CmdletBinding()]
param([string]$Prefix='SpikeRoot-20261111',[string]$ReferenceManifest='')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot "Benchmarks/$Prefix"
[IO.Directory]::CreateDirectory($root)|Out-Null
$progress=Join-Path $root 'progress.txt'
function Note($s) { "$(Get-Date -Format o) $s" | Tee-Object -FilePath $progress -Append }
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if(!([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator is required only for OS context-switch ETW recording. Run this reviewed script through Windows UAC.' }
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$xperf='C:/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit/xperf.exe'
if(!$ReferenceManifest){$ReferenceManifest=Join-Path $PSScriptRoot 'Benchmarks/AnimTasks-20261110/trace-RTS-0/manifest.json'}
$reference=Get-Content $ReferenceManifest -Raw|ConvertFrom-Json
if($reference.units -ne 1000 -or $reference.view -ne 'RTS' -or $reference.decision -ne 'Leader' -or $reference.layout -ne 'Open' -or $reference.seed -ne 1 -or !$reference.stable -or $reference.warmup -ne 10 -or $reference.duration -ne 60){throw 'Reference does not match the approved condition'}
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$lock=$null
try {
 if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue) {throw 'An Unreal process is already running'}
 if(Get-Process cl,link -ErrorAction SilentlyContinue) {throw 'A compiler/linker is running; do not measure concurrently'}
 $lock=[IO.File]::Open((Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 foreach($name in @('control-r1','trace-r1','trace-r2','control-r2')) {
  $out=Join-Path $root $name
  if(Test-Path (Join-Path $out 'manifest.json')) {
   $previous=Get-Content (Join-Path $out 'manifest.json') -Raw|ConvertFrom-Json
   if($previous.status -eq 'passed' -and $previous.dll_sha256 -eq $reference.dll_sha256) {Note "$name already passed; retained";continue}
   if($previous.pid){throw "An attempted game run already exists: $name"}
   $name+='-retry1';$out=Join-Path $root $name
   if(Test-Path $out){throw "Do not overwrite $out"}
  }
  if((Get-FileHash $dll).Hash -ne $reference.dll_sha256) {throw 'DLL changed'}
  foreach($h in @($reference.source_hashes)+@($reference.config_hashes)) {if((Get-FileHash -LiteralPath $h.Path).Hash -ne $h.Hash){throw "Changed: $($h.Path)"}}
  [IO.Directory]::CreateDirectory($out)|Out-Null
  $trace=$name.StartsWith('trace-')
  $runArgs=@($reference.arguments | Where-Object {$_ -notmatch '^-trace=|^-tracefile=|^-statnamedevents$|^-abslog=|^-BattlePhase3=|^-BattleDisplayLOD='})
  $runArgs+='-BattleDisplayLOD=0'
  $runArgs+=@(('-abslog="'+(Join-Path $out 'run.log')+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'))
  if($trace) {$runArgs+=@('-trace=cpu,frame,bookmark,log,task','-statnamedevents',('-tracefile="'+(Join-Path $out 'cpu.utrace')+'"'))}
  $m=@{schema=1;status='prepared';machine=(Get-MeasurementMachine);created=(Get-Date -Format o);dll_sha256=$reference.dll_sha256;arguments=$runArgs;reference='AnimTasks-20261110/trace-RTS-0';units=1000;decision='Leader';view='RTS';layout='Open';seed=1;stable=$true;warmup=10;duration=60;context_reuse=1;hidden_bones=1;perception_grid=1;anim_tasks=0;sense='B';cpu_trace=$trace;task_trace=$trace;etw=$trace;administrator=$true;source_hashes=$reference.source_hashes;config_hashes=$reference.config_hashes;map_hash=$reference.map_hash;runner_sha256=(Get-FileHash $PSCommandPath).Hash}
  $m.reference=[IO.Path]::GetFullPath($ReferenceManifest)
  $m.display_lod=0
  $m.project_hash=(Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject')).Hash
  if($reference.asset_hashes){$m.asset_hashes=$reference.asset_hashes}
  $m.saved_config_before=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Saved/Config') -Filter '*.ini' -Recurse -File | Get-FileHash | Select-Object Path,Hash)
  $topology=& (Join-Path $PSScriptRoot 'Benchmarks/SpikeRoot-20261111/tools/SpikeEtwDecoder.exe') --topology
  if($LASTEXITCODE -ne 0){throw 'CPU topology query failed'}
  $topology|Set-Content (Join-Path $out 'cpu-topology.csv') -Encoding utf8
  $m.cpu_topology=@($topology|ConvertFrom-Csv)
  $m.background_cpu_before=@(Get-Process | Select-Object Id,ProcessName,CPU)
  $m|ConvertTo-Json -Depth 12|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
  $ownedETW=$false
  try {
   if($trace) {
    $active=& $xperf -loggers 2>&1
    $active|Set-Content (Join-Path $out 'etw-before.txt')
    if(($active -join "`n") -match '(?m)^Logger Name\s*:\s*NT Kernel Logger\s*$'){throw 'Existing NT Kernel Logger detected; will not stop or replace it'}
    & $xperf -on PROC_THREAD+LOADER+CSWITCH+DISPATCHER+DPC+INTERRUPT+PROFILE -stackwalk Profile -BufferSize 1024 -MinBuffers 64 -MaxBuffers 256 -f (Join-Path $out 'kernel.etl') 2>&1 | Set-Content (Join-Path $out 'etw-start.txt')
    if($LASTEXITCODE -ne 0){throw 'ETW start failed'}
    $ownedETW=$true
   }
   $m.launch_qpc=[Diagnostics.Stopwatch]::GetTimestamp();$m.qpc_frequency=[Diagnostics.Stopwatch]::Frequency
   $m.launch_utc=[DateTime]::UtcNow.ToString('o')
   $proc=Start-Process $exe -ArgumentList $runArgs -WindowStyle Hidden -PassThru
   $m.pid=$proc.Id;$m.status='running'
   $m|ConvertTo-Json -Depth 12|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
   Note "$name started pid=$($proc.Id)"
   if(!$proc.WaitForExit(250000)){Stop-Process -Id $proc.Id;throw 'Diagnostic game run timed out'}
   $m.exit_code=$proc.ExitCode;$m.exit_qpc=[Diagnostics.Stopwatch]::GetTimestamp()
   try {$m.process_cpu_total_ms=$proc.TotalProcessorTime.TotalMilliseconds} catch {$m.process_cpu_note=$_.ToString()}
   $m.background_cpu_after=@(Get-Process | Select-Object Id,ProcessName,CPU)
   $text=Get-Content (Join-Path $out 'run.log') -Raw
   if($proc.ExitCode -ne 0 -or $text -notmatch 'P3_RESULT failures=0' -or $text -match 'Fatal error:|Assertion failed:|Ensure condition failed:'){throw 'Game run failed'}
   $m.result=@($text -split "`n"|Where-Object {$_ -match 'P3_RESULT|P3_BATCH_LATENESS|Chosen D3D12 Adapter'})
   $m.effective_render_log=@($text -split "`n"|Where-Object {$_ -match 'LogConfig:.*(Shadow|Lumen|Scalability|ScreenPercentage|ViewDistance)|VAT_DISPLAY|ANIM_TASKS|P3_SENSE'})
   $m.saved_config_after=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Saved/Config') -Filter '*.ini' -Recurse -File | Get-FileHash | Select-Object Path,Hash)
   $m.status='passed'
  } catch {$m.status='failed';$m.error=$_.ToString();throw} finally {
   if($ownedETW) {
    & $xperf -d (Join-Path $out 'os.etl') 2>&1|Set-Content (Join-Path $out 'etw-stop.txt')
    $m.etw_stop_exit=$LASTEXITCODE
   }
   $m.completed=Get-Date -Format o
   $m|ConvertTo-Json -Depth 12|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
  }
  if($trace -and $m.etw_stop_exit -ne 0){throw 'ETW finalization failed'}
  if((Get-FileHash $dll).Hash -ne $reference.dll_sha256){throw 'DLL changed during recording'}
  Note "$name passed"
 }
 Note 'ALL_FOUR_RUNS_COMPLETE'
} catch {Note "FAILED: $_";throw} finally {if($lock){$lock.Dispose()}}
