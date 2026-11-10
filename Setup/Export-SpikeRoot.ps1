param([Parameter(Mandatory=$true)][string]$Folder)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($Folder)
$insights='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe'
$python='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
function Export([string]$rsp,[string]$log) {
 $args=@(('-OpenTraceFile="'+$out+'/cpu.utrace"'),('-ABSLOG="'+$out+'/'+$log+'"'),'-AutoQuit','-NoUI',('-ExecOnAnalysisCompleteCmd="@='+$rsp+'"'))
 $p=Start-Process $insights -ArgumentList $args -WindowStyle Hidden -PassThru
 if(!$p.WaitForExit(300000)){throw 'Offline trace export timeout; inspect the process'}
 if($p.ExitCode -ne 0){throw 'Offline trace export failed'}
}
$rsp=Join-Path $out 'basic-export.rsp'
@(('TimingInsights.ExportThreads "'+$out+'/threads.csv"'),('TimingInsights.ExportTimers "'+$out+'/timers.csv"'),('TimingInsights.ExportTimingEvents "'+$out+'/ticks.csv" -threads=GameThread -timers=FEngineLoop::Tick -columns=TimerId,StartTime,EndTime,Depth'),('TimingInsights.ExportTimingEvents "'+$out+'/gc-events.csv" -threads=GameThread -timers=ConditionalCollectGarbage -columns=TimerId,StartTime,EndTime,Depth')) | ForEach-Object {$_.Replace('\','/')} | Set-Content $rsp -Encoding utf8
Export $rsp 'basic-export.log'
& $python "$PSScriptRoot/Analyze-SpikeRoot.py" prepare $out
if($LASTEXITCODE -ne 0){throw 'Window selection failed'}
Export (Join-Path $out 'windows-export.rsp') 'windows-export.log'
$env:PATH='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64;'+$env:PATH
& "$PSScriptRoot/Benchmarks/SpikeRoot-20261111/tools/SpikeTraceDecoder.exe" "$out/cpu.utrace" $out
if($LASTEXITCODE -ne 0){throw 'Task and clock decoding failed'}
$m=Get-Content "$out/manifest.json" -Raw|ConvertFrom-Json
& "$PSScriptRoot/Benchmarks/SpikeRoot-20261111/tools/SpikeEtwDecoder.exe" "$out/os.etl" $out "$($m.pid)"
if($LASTEXITCODE -ne 0){throw 'OS decoding failed'}
