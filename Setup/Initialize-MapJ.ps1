# Map J (2026-11-22, Docs/design-map-j-terrain.md): generate Lvl_BattleWide_J when it does not exist (BattleMapCommandlet -WideField=J).
param([Parameter(Mandatory)][string]$Project,[switch]$Force) # -Force regenerates an existing map J (the commandlet allows it for urban maps)
$ErrorActionPreference='Stop'
$mapFile = Join-Path (Split-Path $Project) 'Content/Prototype/Lvl_BattleWide_J.umap'
if ($Force -or !(Test-Path -LiteralPath $mapFile)) {
    $log = Join-Path $PSScriptRoot ("Create-MapJ-{0}.log" -f [guid]::NewGuid())
    $editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $argsForMap = @(('"' + $Project + '"'), '-run=BattleMap', '-WideField=J', $(if($Force){'-Force'}else{'-unattended'}), '-nullrhi', '-unattended', '-nosound', ('-abslog="' + $log + '"'))
    $startupLog = [IO.Path]::ChangeExtension($log, 'startup.txt')
    "Starting Map J generation at $([DateTimeOffset]::Now.ToString('o'))" | Set-Content -LiteralPath $startupLog
    try {
        $creation = Start-Process $editor -ArgumentList $argsForMap -WindowStyle Hidden -PassThru
        $creation.WaitForExit()
    } catch {
        Add-Content -LiteralPath $startupLog -Value $_.Exception.Message
        throw
    }
    Add-Content -LiteralPath $startupLog -Value "Editor exit: $($creation.ExitCode)"
    if ($creation.ExitCode -ne 0 -or !(Test-Path -LiteralPath $mapFile) -or !(Test-Path -LiteralPath $log) -or
        !(Select-String -LiteralPath $log -SimpleMatch 'BATTLE_MAP_CREATED' -Quiet)) {
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
        throw "Map J creation failed. $reason Startup report: $startupLog"
    }
    Add-Content -LiteralPath $startupLog -Value "Created: $mapFile"
}
