param(
    [string]$BuildDir,
    [string]$ExternalsRoot,
    [string]$Platform = "windows-x64",
    [switch]$Monolith,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir ".."))
if ($Help) {
    @"
Usage: powershell -File scripts/run-xq.ps1 [options]

Options:
  -BuildDir <path>        Build directory. Defaults to build/windows-msvc-release.
  -ExternalsRoot <path>   Root of the Externals entry repository.
  -Platform windows-x64   Externals platform suffix.
  -Monolith               Run XQMonolith.exe instead of XQ.exe.
"@
    exit 0
}

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build\windows-msvc-release"
}

$envScript = Join-Path $ScriptDir "xq-env.ps1"
$envParams = @{
    XQRoot = $RepoRoot
    BuildDir = $BuildDir
    Platform = $Platform
}
if ($ExternalsRoot) {
    $envParams.ExternalsRoot = $ExternalsRoot
}
. $envScript @envParams | Out-Null

$exeName = if ($Monolith) { "XQMonolith.exe" } else { "XQ.exe" }
$exe = Join-Path $BuildDir (Join-Path "bin" $exeName)
if (-not (Test-Path -LiteralPath $exe)) {
    throw "[XQ][ERROR] XQ executable not found: $exe"
}

& $exe @args
