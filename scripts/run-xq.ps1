param(
    [string]$BuildDir,
    [string]$ExternalsRoot,
    [string]$Platform,
    [switch]$Monolith,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir ".."))
. (Join-Path $ScriptDir "xq-dotenv.ps1")
Import-XQDotEnv -RepoRoot $RepoRoot | Out-Null
if ($Help) {
    @"
Usage: powershell -File scripts/run-xq.ps1 [options]

Options:
  -BuildDir <path>        Build directory. Defaults to build/windows-msvc-release.
  -ExternalsRoot <path>   Root of the Externals entry repository.
  -Platform windows-x64   Externals platform suffix.
  -Monolith               Prefer XQMonolith.exe, then fall back to XQ.exe.
"@
    exit 0
}

if (-not $BuildDir) {
    $BuildDir = if ($env:XQ_BUILD_DIR) { $env:XQ_BUILD_DIR } else { Join-Path $RepoRoot "build\windows-msvc-release" }
}
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $BuildDir))
}
else {
    $BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
}
if (-not $ExternalsRoot -and $env:XQ_EXTERNALS_ROOT) {
    $ExternalsRoot = $env:XQ_EXTERNALS_ROOT
}
if (-not $Platform -and $env:XQ_EXTERNALS_PLATFORM) {
    $Platform = $env:XQ_EXTERNALS_PLATFORM
}
if (-not $Platform) {
    $Platform = "windows-x64"
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

$exeCandidates = if ($Monolith) {
    @("XQMonolith.exe", "XQ.exe")
} else {
    @("XQ.exe")
}

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
