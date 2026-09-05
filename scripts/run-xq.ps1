param(
    [string]$BuildDir,
    [string]$ExternalsRoot,
    [string]$Platform = "windows-x64-blueberry",
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
  -Platform <name>         Externals platform suffix. Defaults to windows-x64-blueberry.
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

$exeCandidates = @("XQ.exe")

$checkedPaths = @()
$exe = $null
foreach ($candidate in $exeCandidates) {
    $candidatePath = Join-Path $BuildDir (Join-Path "bin" $candidate)
    $checkedPaths += $candidatePath
    if (Test-Path -LiteralPath $candidatePath) {
        $exe = $candidatePath
        break
    }
}

if (-not $exe) {
    throw "[XQ][ERROR] XQ executable not found. Checked: $($checkedPaths -join ', ')"
}

& $exe @args
