param([Parameter(Mandatory=$true)][string]$Folder,[switch]$FullEvents)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($Folder)
$rsp=Join-Path $out 'export.rsp'
@(
 ('TimingInsights.ExportThreads "'+(Join-Path $out 'threads.csv')+'"'),
 ('TimingInsights.ExportTimers "'+(Join-Path $out 'timers.csv')+'"'),
 ('TimingInsights.ExportTimingEvents "'+(Join-Path $out 'gt-events.csv')+'" -threads=GameThread -timers=FEngineLoop*,*World_Tick*,*GameMode*,*DrawHUD* -columns=ThreadId,TimerId,TimerName,StartTime,EndTime,Depth'),
 ('TimingInsights.ExportTimerStatistics "'+(Join-Path $out 'gt-statistics.csv')+'" -threads=GameThread')
) | ForEach-Object { $_.Replace('\','/') } | Set-Content -LiteralPath $rsp -Encoding utf8
$args=@(('-OpenTraceFile="'+(Join-Path $out 'cpu.utrace')+'"'),('-ABSLOG="'+(Join-Path $out 'export.log')+'"'),'-AutoQuit','-NoUI',('-ExecOnAnalysisCompleteCmd="@='+$rsp+'"'))
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe' -ArgumentList $args -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(180000)) { Stop-Process -Id $p.Id; throw 'Trace export timeout' }
if($p.ExitCode -ne 0 -or !(Test-Path (Join-Path $out 'gt-events.csv'))) { throw 'Trace export failed' }
Get-Item (Join-Path $out 'gt-events.csv') | Select-Object FullName,Length
# Sample() records the preceding frame. Exclude the final Finish() frame and
# select exactly the preceding N complete engine ticks (N = measured CSV rows).
$n=@(Import-Csv (Join-Path $out 'frames.csv')).Count
$ticks=@(Import-Csv (Join-Path $out 'gt-events.csv') | Where-Object TimerName -eq 'FEngineLoop::Tick')
if($ticks.Count -lt $n+1) { throw 'Not enough complete engine ticks' }
$first=$ticks[$ticks.Count-$n-1]; $last=$ticks[$ticks.Count-2]
$start=$first.StartTime; $end=$last.EndTime
@{frames=$n;start=[double]$start;end=[double]$end;selection='N complete engine ticks preceding the final Finish frame; Sample records prior-frame costs'} | ConvertTo-Json | Set-Content (Join-Path $out 'window.json')
$exportFull=$FullEvents -or [bool](Get-Content (Join-Path $out 'manifest.json') -Raw | ConvertFrom-Json).light_trace
$commands=@(('TimingInsights.ExportTimerStatistics "'+(Join-Path $out 'window-statistics.csv')+'" -threads=GameThread -startTime='+$start+' -endTime='+$end))
if($exportFull) { $commands+=('TimingInsights.ExportTimingEvents "'+(Join-Path $out 'window-events.csv')+'" -threads=GameThread -columns=TimerId,StartTime,EndTime,Depth -startTime='+$start+' -endTime='+$end) }
$commands | ForEach-Object { $_.Replace('\','/') } | Set-Content -LiteralPath $rsp -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe' -ArgumentList $args -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(300000)) { Stop-Process -Id $p.Id; throw 'Window export timeout' }
if($p.ExitCode -ne 0 -or !(Test-Path (Join-Path $out 'window-statistics.csv'))) { throw 'Window export failed' }
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' (Join-Path $PSScriptRoot 'Analyze-GTTrace.py') $out
if($LASTEXITCODE -ne 0) { throw 'Trace accounting failed' }
