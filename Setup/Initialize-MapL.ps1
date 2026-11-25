# Map L (2026-11-25, Docs/design-terrain-field.md): generate Lvl_BattleWide_L (map I on rolling ground) and its ground meshes
# (Content/Prototype/Terrain/L) when the map does not exist (BattleMapCommandlet -WideField=L).
param([Parameter(Mandatory)][string]$Project,[switch]$Force) # -Force regenerates an existing map L (the commandlet allows it for urban maps)
$ErrorActionPreference='Stop'
$mapFile = Join-Path (Split-Path $Project) 'Content/Prototype/Lvl_BattleWide_L.umap'
if ($Force -or !(Test-Path -LiteralPath $mapFile)) {
    $log = Join-Path $PSScriptRoot ("Create-MapL-{0}.log" -f [guid]::NewGuid())
    $editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $argsForMap = @(('"' + $Project + '"'), '-run=BattleMap', '-WideField=L', $(if($Force){'-Force'}else{'-unattended'}), '-nullrhi', '-unattended', '-nosound', ('-abslog="' + $log + '"'))
    $startupLog = [IO.Path]::ChangeExtension($log, 'startup.txt')
    "Starting Map L generation at $([DateTimeOffset]::Now.ToString('o'))" | Set-Content -LiteralPath $startupLog
    try {
        $creation = Start-Process $editor -ArgumentList $argsForMap -WindowStyle Hidden -PassThru
        $creation.WaitForExit()
    } catch {
        Add-Content -LiteralPath $startupLog -Value $_.Exception.Message
        throw
    }
    Add-Content -LiteralPath $startupLog -Value "Editor exit: $($creation.ExitCode)"
    if ($creation.ExitCode -ne 0 -or !(Test-Path -LiteralPath $mapFile) -or !(Test-Path -LiteralPath $log) -or
        !(Select-String -LiteralPath $log -SimpleMatch 'BATTLE_MAP_CREATED' -Quiet) -or
        !(Select-String -LiteralPath $log -SimpleMatch 'TERRAIN_FIELD_RESULT failures=0' -Quiet)) {
        if (Test-Path -LiteralPath $log) {
            $reason = "Editor started. See generation and navigation checks in $log"
        } else {
            $reason = 'Editor exited before producing its log.'
            try {
                $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Epic Games\Unreal Engine\Identifiers')
                if ($null -ne $key) {
                    try {
                        $id = [guid]::Empty
                        $valid = [guid]::TryParse([string]$key.GetValue('MachineId'), [ref]$id)
                        $reason += " Readable nonempty MachineId: $($valid -and $id -ne [guid]::Empty)."
                    } finally { $key.Dispose() }
                } else {
                    $reason += ' Unreal Engine has not initialized the user identifiers key.'
                }
            } catch [System.Security.SecurityException] {
                $reason += ' Access to the Unreal Engine user identifiers key is denied. Run Map-H-TestPlay.lnk from Windows Explorer under the signed-in user.'
            } catch [System.UnauthorizedAccessException] {
                $reason += ' Windows denied access to the Unreal Engine user identifiers key. Run Map-H-TestPlay.lnk from Windows Explorer under the signed-in user.'
            }
        }
        Add-Content -LiteralPath $startupLog -Value $reason
        throw "Map L creation failed. $reason Startup report: $startupLog"
    }
    Add-Content -LiteralPath $startupLog -Value "Created: $mapFile"
}
