# Opens the editor with the UE 5.8 MCP server (ModelContextProtocol + AllToolsets) for this session only.
# The plugins are enabled on the command line, so FutureBattle.uproject, Config and match runs are unchanged.
# Each work tree that opens an MCP editor at the same time needs its own -Port.
# The editor loads this work tree's DLL: Build-Editor.ps1 for the same work tree fails while it is open.
[CmdletBinding()]
param(
    [int]$Port = 8000,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$Map = '',
    [int]$WaitSeconds = 300,
    [switch]$NoWait
)
$ErrorActionPreference = 'Stop'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
if (!(Test-Path -LiteralPath $editor)) { throw "Editor missing: $editor" }
if (!(Test-Path -LiteralPath $project)) { throw "Project missing: $project" }
if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) {
    throw "Port $Port is already in use (another MCP editor?). Pass -Port <other>."
}

$arguments = @(('"' + $project + '"'))
if ($Map) { $arguments += $Map }
$arguments += @('-EnablePlugins=ModelContextProtocol,AllToolsets', '-ModelContextProtocolStartServer', "-ModelContextProtocolPort=$Port")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru
$url = "http://127.0.0.1:$Port/mcp"
Write-Output "MCP_EDITOR pid=$($process.Id) project=$project url=$url"
if ($NoWait) { exit 0 }

$deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $deadline) {
    if ($process.HasExited) { throw "Editor exited early (code $($process.ExitCode))." }
    if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) {
        Write-Output "MCP_READY url=$url"
        exit 0
    }
    Start-Sleep -Seconds 2
}
throw "MCP server did not start listening on port $Port within $WaitSeconds s (editor pid $($process.Id) still running)."
