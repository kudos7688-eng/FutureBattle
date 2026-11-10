# Shared by measurement runners. Collect once before launching the measured process.
function Get-MeasurementMachine {
    [CmdletBinding()]
    param()

    $gpus = @(foreach ($gpu in (Get-CimInstance Win32_VideoController -ErrorAction Stop)) {
        $vramBytes = $null
        $vramSource = $null
        $vramNote = $null
        try {
            # AdapterRAM is uint32 and truncates large GPUs. Match the registry
            # adapter through its PnP driver key, rather than relying on GPU order.
            $device = Get-ItemProperty -LiteralPath ("HKLM:\SYSTEM\CurrentControlSet\Enum\" + $gpu.PNPDeviceID) -ErrorAction Stop
            $adapter = Get-ItemProperty -LiteralPath ("HKLM:\SYSTEM\CurrentControlSet\Control\Class\" + $device.Driver) -ErrorAction Stop
            $memory = $adapter.'HardwareInformation.qwMemorySize'
            if ($memory -is [byte[]] -and $memory.Length -eq 8) {
                $memory = [BitConverter]::ToUInt64($memory, 0)
            }
            if ($null -ne $memory -and [uint64]$memory -gt 0) {
                $vramBytes = [uint64]$memory
                $vramSource = 'Registry HardwareInformation.qwMemorySize'
            }
        } catch {
            $vramNote = '64-bit driver memory value unavailable: ' + $_.Exception.Message
        }
        if ($null -eq $vramBytes) {
            # Preserve the raw CIM value below, but never label a potentially
            # truncated uint32 value as the actual VRAM capacity.
            $vramNote = 'VRAM unknown; CIM AdapterRAM is uint32 and may be truncated. ' + $vramNote
            Write-Warning ("{0}: {1}" -f $gpu.Name, $vramNote)
        }
        [ordered]@{
            name = $gpu.Name
            pnp_device_id = $gpu.PNPDeviceID
            driver_version = $gpu.DriverVersion
            vram_bytes = $vramBytes
            vram_source = $vramSource
            vram_note = $vramNote
            cim_adapter_ram_bytes = $gpu.AdapterRAM
        }
    })
    $cpus = @(foreach ($cpu in (Get-CimInstance Win32_Processor -ErrorAction Stop)) {
        [ordered]@{
            name = $cpu.Name
            cores = $cpu.NumberOfCores
            logical_processors = $cpu.NumberOfLogicalProcessors
        }
    })
    $system = Get-CimInstance Win32_ComputerSystem -ErrorAction Stop
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    if (!$gpus.Count -or !$cpus.Count -or !$system.TotalPhysicalMemory -or !$os.Caption) {
        throw 'Measurement machine inventory is incomplete.'
    }
    [ordered]@{
        schema_version = 1
        captured_at_utc = [DateTime]::UtcNow.ToString('o')
        gpus = $gpus
        cpus = $cpus
        memory_bytes = [uint64]$system.TotalPhysicalMemory
        os = [ordered]@{
            name = $os.Caption
            version = $os.Version
            build_number = $os.BuildNumber
            architecture = $os.OSArchitecture
        }
    }
}
