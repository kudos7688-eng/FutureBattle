[CmdletBinding()]
# Exports per-timer statistics of one thread (default GameThread) from an Unreal Insights trace to CSV, without the Insights UI.
# Example: .\Setup\Export-TraceTimers.ps1 -Trace .\Setup\Benchmarks\SightTrial-20261111\<run>\trace.utrace -StartTime 120 -EndTime 840
param([Parameter(Mandatory)][string]$Trace,[string]$Threads='GameThread',[double]$StartTime=-1,[double]$EndTime=-1,[string]$Output='')
$ErrorActionPreference='Stop'
$Trace=[IO.Path]::GetFullPath($Trace)
if(-not (Test-Path $Trace)){throw "Trace not found: $Trace"}
if($Threads -notmatch '^[A-Za-z0-9_,*]+$'){throw 'Threads: names, commas and * only'}
if(-not $Output){$Output=[IO.Path]::ChangeExtension($Trace,$null).TrimEnd('.')+'-'+($Threads -replace '[,*]','_')+'.csv'}
$Output=[IO.Path]::GetFullPath($Output)
$log=[IO.Path]::ChangeExtension($Output,'.log')
$insights='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealInsights.exe'
$cmd="TimingInsights.ExportTimerStatistics `"$($Output -replace '\\','/')`" -threads=`"$Threads`" -sortBy=TotalInclusiveTime"
if($StartTime -ge 0){$cmd+=" -startTime=$StartTime"};if($EndTime -ge 0){$cmd+=" -endTime=$EndTime"}
# The command goes through a command file (@=path): nested quotes on the command line would split it.
$cmdFile=[IO.Path]::ChangeExtension($Output,'.cmd.txt');[IO.File]::WriteAllText($cmdFile,$cmd)
$insArgs=@("-OpenTraceFile=`"$Trace`"","-ABSLOG=`"$log`"",'-AutoQuit','-NoUI',"-ExecOnAnalysisCompleteCmd=`"@=$cmdFile`"",'-log')
$p=Start-Process $insights -ArgumentList $insArgs -PassThru -WindowStyle Hidden
if(-not $p.WaitForExit(1800000)){throw "Insights did not finish; see $log"}
if(-not (Test-Path $Output)){throw "No export written; see $log"}
Write-Output $Output
