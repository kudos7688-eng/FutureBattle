@echo off
setlocal
set "taskVsRoot=C:\BuildTools"
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "taskVsRoot=%%i"
call "%taskVsRoot%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist "Output" mkdir "Output"
cl /nologo /EHsc /std:c++20 CompilerSmoke.cpp /Fo:Output\CompilerSmoke.obj /Fe:Output\CompilerSmoke.exe
if errorlevel 1 exit /b 1
Output\CompilerSmoke.exe
set "taskResult=%ERRORLEVEL%"
popd
exit /b %taskResult%
