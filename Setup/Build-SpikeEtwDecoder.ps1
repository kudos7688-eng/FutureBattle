$ErrorActionPreference='Stop'
$vc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207'
$sdk='C:/Program Files (x86)/Windows Kits/10'
$out=Join-Path $PSScriptRoot 'Benchmarks/SpikeRoot-20261111/tools'
[IO.Directory]::CreateDirectory($out)|Out-Null
& "$vc/bin/Hostx64/x64/cl.exe" /nologo /std:c++20 /EHsc /O2 /MD /D_CRT_SECURE_NO_WARNINGS ('/I'+"$vc/include") ('/I'+"$sdk/Include/10.0.26100.0/ucrt") ('/I'+"$sdk/Include/10.0.26100.0/um") ('/I'+"$sdk/Include/10.0.26100.0/shared") (Join-Path $PSScriptRoot 'SpikeEtwDecoder.cpp') ('/Fo'+"$out/SpikeEtwDecoder.obj") ('/Fe'+"$out/SpikeEtwDecoder.exe") /link ('/LIBPATH:'+"$vc/lib/x64") ('/LIBPATH:'+"$sdk/Lib/10.0.26100.0/ucrt/x64") ('/LIBPATH:'+"$sdk/Lib/10.0.26100.0/um/x64") tdh.lib advapi32.lib
if($LASTEXITCODE -ne 0){throw 'Offline ETW reader build failed'}
