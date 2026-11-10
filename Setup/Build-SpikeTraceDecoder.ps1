# Builds only an offline trace reader; never invokes the game build.
$ErrorActionPreference='Stop'
$engine='C:/Program Files/Epic Games/UE_5.8/Engine'
$repo=Split-Path $PSScriptRoot
$out=Join-Path $PSScriptRoot 'Benchmarks/SpikeRoot-20261111/tools'
[IO.Directory]::CreateDirectory($out)|Out-Null
$vc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207'
$shared=Join-Path $repo 'Game/FutureBattle/Intermediate/Build/Win64/x64/UnrealEditor/Development/FutureBattle/FutureBattle.Shared.rsp'
$defines=Join-Path $repo 'Game/FutureBattle/Intermediate/Build/Win64/x64/UnrealEditor/Development/FutureBattle/Definitions.FutureBattle.h'
$options=@(Get-Content $shared | Where-Object {$_ -notmatch '^/we|^/WX|^/D_WINDLL$'} | ForEach-Object {$_.Replace('C:/BuildTools/VC/Tools/MSVC/14.44.35207/INCLUDE',"$vc/include")})
$options+=@('/std:c++20','/DTRACEANALYSIS_API=__declspec(dllimport)','/DTRACEANALYSIS_NON_ATTRIBUTED_API=__declspec(dllimport)',('/FI"'+$defines+'"'),('/I "'+$engine+'/Source/Developer/TraceAnalysis/Public"'),('/I "'+$engine+'/Source/Runtime/TraceLog/Public"'),('/Fo"'+$out+'/SpikeTraceDecoder.obj"'),('"'+$PSScriptRoot+'/SpikeTraceDecoder.cpp"'))
$rsp=Join-Path $out 'compile.rsp';$options|Set-Content $rsp -Encoding utf8
Push-Location "$engine/Source"
try { & "$vc/bin/Hostx64/x64/cl.exe" ('@'+$rsp);if($LASTEXITCODE -ne 0){throw 'Offline reader compile failed'} } finally {Pop-Location}
$libs=@('Core','TraceAnalysis','TraceLog')|ForEach-Object {"$engine/Intermediate/Build/Win64/x64/UnrealEditor/Development/$_/UnrealEditor-$_.lib"}
& "$vc/bin/Hostx64/x64/link.exe" /nologo ("$out/SpikeTraceDecoder.obj") ('/OUT:'+(Join-Path $out 'SpikeTraceDecoder.exe')) @libs ('/LIBPATH:'+"$vc/lib/x64") '/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64' '/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64' kernel32.lib user32.lib advapi32.lib
if($LASTEXITCODE -ne 0){throw 'Offline reader link failed'}
