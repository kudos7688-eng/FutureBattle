# Existing .utrace analysis only: does not launch the game or change its settings.
$ErrorActionPreference='Stop'
$python='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $python "$PSScriptRoot/Analyze-SpikeTrace.py" prepare
if($LASTEXITCODE -ne 0) { throw 'Selection failed' }
$base=Join-Path $PSScriptRoot 'Benchmarks'
foreach($run in @('AnimTasks-20261110/trace-RTS-0','AnimTasks-20261110/trace-RTS-1','GTBothOn-20261110/RTS-light','GTBothOn-20261110/TPS-light')) {
    $dest=Join-Path "$base/FrameSpikes-20261110" ($run.Replace('/','_'))
    $arguments=@('-OpenTraceFile="'+"$base/$run/cpu.utrace"+'"','-ABSLOG="'+"$dest/export.log"+'"','-AutoQuit','-NoUI','-ExecOnAnalysisCompleteCmd="@='+"$dest/export.rsp"+'"')
    $process=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    if($process.ExitCode -ne 0) { throw "Export failed: $run" }
}
& $python "$PSScriptRoot/Analyze-SpikeTrace.py" analyze
if($LASTEXITCODE -ne 0) { throw 'Accounting failed' }
