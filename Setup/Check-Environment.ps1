[CmdletBinding()]
param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$instances = @(& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json -utf8 | ConvertFrom-Json)
foreach ($instance in $instances) {
    Write-Output "Visual Studio: $($instance.displayName) $($instance.installationVersion)"
    Write-Output "  Path: $($instance.installationPath); complete: $($instance.isComplete); reboot: $($instance.isRebootRequired)"
}
$git = 'C:\Program Files\Git\cmd\git.exe'
if (Test-Path -LiteralPath $git) { & $git --version } else { Write-Warning 'Standalone Git not installed' }
$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10\Include'
Get-ChildItem -LiteralPath $sdkRoot -Directory | ForEach-Object { Write-Output "Windows SDK: $($_.Name)" }
$launcher = 'C:\Program Files\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe'
Write-Output "Epic Games Launcher present: $(Test-Path -LiteralPath $launcher)"
if (!$EngineRoot) {
    $registration = 'C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path -LiteralPath $registration) {
        $engines = @((Get-Content -LiteralPath $registration -Raw | ConvertFrom-Json).InstallationList | Where-Object AppName -Like 'UE_5.7*')
        if ($engines.Count -eq 1) { $EngineRoot = $engines[0].InstallLocation }
    }
}
if (!$EngineRoot) {
    Write-Warning 'UE 5.7 not registered. Install via Epic Games Launcher, then rerun; use -EngineRoot for a custom install.'
    exit 2
}
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$version = Join-Path $EngineRoot 'Engine\Build\Build.version'
foreach ($file in @($editor, $build, $version)) {
    if (!(Test-Path -LiteralPath $file)) { throw "Missing engine file: $file" }
}
Write-Output "UE root: $EngineRoot"
Get-Content -LiteralPath $version
Write-Output 'Engine files found. A real project build and Editor Play are still required.'

