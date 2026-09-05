param(
    [string]$Profile = "xq",
    [string]$Target = "all",
    [string]$Platform = "windows-x64",
    [string]$BuildType = "Release",
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 1),
    [string]$VsInstallPath,
    [switch]$ConfigureOnly,
    [switch]$SkipVisualStudioInit,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $RootDir) {
    $RootDir = (Get-Location).Path
}
$RootDir = [System.IO.Path]::GetFullPath($RootDir)
$ScriptDir = Join-Path $RootDir "scripts"

function Show-Usage {
    @"
Usage: pwsh build_all.ps1 [-Profile xq|xq-blueberry] [-Target all|Dependency] [options]

Profiles:
  xq             Build the Windows dependency stack expected by XQ monolith.
  xq-blueberry   Build the legacy BlueBerry stack for XQ1.

Options:
  -Target <name>          Build one dependency or all dependencies.
  -Platform <name>        Platformed install/build directory suffix.
  -BuildType Release      CMake build type.
  -Jobs <n>               Parallel build jobs.
  -ConfigureOnly          Run configure smoke checks without building.
  -SkipVisualStudioInit   Do not import the VS2022 developer environment.
  -VsInstallPath <path>   Use a specific Visual Studio 2022 installation.
  -Help                   Show this help.

Before building, fetch sources with:
  pwsh scripts/Fetch-Sources.ps1
"@
}

if ($Help -or $Profile -in @("help", "-h", "--help")) {
    Show-Usage
    exit 0
}

$supportedProfiles = @("xq", "xq-blueberry")
if ($supportedProfiles -notcontains $Profile) {
    Write-Error "[Externals][ERROR] Unknown profile: $Profile"
    Show-Usage
    exit 2
}

if ($Profile -eq "xq-blueberry" -and -not $PSBoundParameters.ContainsKey("Platform")) {
    $Platform = "windows-x64-blueberry"
}

. (Join-Path $RootDir "env_variables.ps1") -RootDir $RootDir -Platform $Platform
. (Join-Path $ScriptDir "Build-Helpers.ps1")

$ManifestPath = Join-Path $RootDir "externals.manifest"

if (-not (Test-XQSourceTree -RootDir $RootDir -ManifestPath $ManifestPath)) {
    exit 3
}

New-Item -ItemType Directory -Path $XQExternalPaths.BuildRoot -Force | Out-Null
New-Item -ItemType Directory -Path $XQExternalPaths.InstallRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $RootDir "output") -Force | Out-Null

if (-not $SkipVisualStudioInit) {
    . (Join-Path $ScriptDir "Initialize-VS2022.ps1") -VsInstallPath $VsInstallPath -Architecture "x64"
}

$entries = Get-XQManifestEntries -ManifestPath $ManifestPath
foreach ($entry in $entries) {
    if ($Target -ne "all" -and
        -not $entry.Key.Equals($Target, [StringComparison]::OrdinalIgnoreCase) -and
        -not $entry.Name.Equals($Target, [StringComparison]::OrdinalIgnoreCase)) {
        continue
    }

    & (Join-Path $ScriptDir "Build-Dependency.ps1") -Profile $Profile `
        -Dependency $entry.Key `
        -RootDir $RootDir `
        -Platform $Platform `
        -BuildType $BuildType `
        -Jobs $Jobs `
        -ConfigureOnly:$ConfigureOnly
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
