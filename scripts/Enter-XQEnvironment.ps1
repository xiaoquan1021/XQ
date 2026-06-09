param(
    [string]$BuildDir,
    [string]$ExternalsRoot,
    [string]$Platform,
    [string]$VsInstallPath,
    [string]$CMake,
    [switch]$NoVisualStudio
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir ".."))

. (Join-Path $ScriptDir "xq-dotenv.ps1")
. (Join-Path $ScriptDir "xq-toolchain.ps1")
Import-XQDotEnv -RepoRoot $RepoRoot | Out-Null

if (-not $BuildDir -and $env:XQ_BUILD_DIR) {
    $BuildDir = $env:XQ_BUILD_DIR
}
if (-not $ExternalsRoot -and $env:XQ_EXTERNALS_ROOT) {
    $ExternalsRoot = $env:XQ_EXTERNALS_ROOT
}
if (-not $Platform) {
    $Platform = if ($env:XQ_EXTERNALS_PLATFORM) { $env:XQ_EXTERNALS_PLATFORM } else { "windows-x64" }
}
if (-not $VsInstallPath -and $env:XQ_VS_INSTALL_PATH) {
    $VsInstallPath = $env:XQ_VS_INSTALL_PATH
}
if (-not $CMake -and $env:XQ_CMAKE) {
    $CMake = $env:XQ_CMAKE
}

if (-not $NoVisualStudio) {
    $VsPath = Find-XQVisualStudioInstall -PreferredPath $VsInstallPath
    Import-XQVisualStudioEnvironment -InstallPath $VsPath
    $env:XQ_VS_INSTALL_PATH = $VsPath
}

$envScript = Join-Path $ScriptDir "xq-env.ps1"
$envParams = @{
    XQRoot = $RepoRoot
    Platform = $Platform
}
if ($BuildDir) {
    $envParams.BuildDir = $BuildDir
}
if ($ExternalsRoot) {
    $envParams.ExternalsRoot = $ExternalsRoot
}

$resolved = . $envScript @envParams
if ($CMake) {
    $env:XQ_CMAKE = $CMake
}
elseif (-not $NoVisualStudio) {
    $env:XQ_CMAKE = Resolve-XQCMake -RequestedCMake $null -VsPath $env:XQ_VS_INSTALL_PATH
}

$resolved
