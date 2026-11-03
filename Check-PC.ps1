# Read-only Windows development environment check. Run in PowerShell.
Write-Host '=== FutureBattle PC Check ==='
Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,@{N='RAM_GB';E={[math]::Round($_.TotalVisibleMemorySize/1MB,1)}} | Format-List
Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | Format-List
Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,@{N='AdapterRAM_GB_Approx';E={[math]::Round($_.AdapterRAM/1GB,1)}} | Format-Table -AutoSize
Get-PSDrive -PSProvider FileSystem | Select-Object Name,@{N='Free_GB';E={[math]::Round($_.Free/1GB,1)}} | Format-Table -AutoSize
foreach($name in @('git','cmake')) { $cmd=Get-Command $name -ErrorAction SilentlyContinue; if($cmd){Write-Host "$name : $($cmd.Source)"}else{Write-Host "$name : not found on PATH"} }
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if(Test-Path $vswhere){& $vswhere -latest -products * -property installationPath}else{Write-Host 'Visual Studio installer detection unavailable'}
Write-Host 'GPU memory reported by WMI may be inaccurate; verify in Task Manager or GPU vendor utility.'
