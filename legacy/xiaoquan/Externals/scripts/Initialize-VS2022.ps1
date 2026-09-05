param(
    [string]$VsInstallPath,
    [string]$Architecture = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$HelperPath = Join-Path $ScriptDir "Build-Helpers.ps1"
if (Test-Path -LiteralPath $HelperPath) {
    . $HelperPath
}

function Find-XQVisualStudioInstall {
    param([string]$PreferredPath)

    if ($PreferredPath -and (Test-Path -LiteralPath $PreferredPath)) {
        return (Resolve-Path -LiteralPath $PreferredPath).Path
    }

    if ($env:VSINSTALLDIR -and (Test-Path -LiteralPath $env:VSINSTALLDIR)) {
        return (Resolve-Path -LiteralPath $env:VSINSTALLDIR).Path
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and $found -and (Test-Path -LiteralPath $found)) {
            return (Resolve-Path -LiteralPath $found).Path
        }
    }

    $knownRoots = @(
        "C:\software\Visual Studio\Visual Studio2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
    )

    foreach ($root in $knownRoots) {
        if (Test-Path -LiteralPath $root) {
            return (Resolve-Path -LiteralPath $root).Path
        }
    }

    throw "[Externals][ERROR] Visual Studio 2022 with MSVC x64 tools was not found."
}

function Import-XQVisualStudioEnvironment {
    param(
        [Parameter(Mandatory)][string]$InstallPath,
        [Parameter(Mandatory)][string]$Architecture
    )

    $vsDevCmd = Join-Path $InstallPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path -LiteralPath $vsDevCmd)) {
        throw "[Externals][ERROR] VsDevCmd.bat not found under $InstallPath"
    }

    $cmd = "`"$vsDevCmd`" -arch=$Architecture -host_arch=x64 && set"
    $envLines = & cmd.exe /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "[Externals][ERROR] Failed to initialize Visual Studio environment."
    }

    foreach ($line in $envLines) {
        $idx = $line.IndexOf("=")
        if ($idx -le 0) {
            continue
        }
        $name = $line.Substring(0, $idx)
        $value = $line.Substring($idx + 1)
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }
}

$ResolvedVsInstallPath = Find-XQVisualStudioInstall -PreferredPath $VsInstallPath
Import-XQVisualStudioEnvironment -InstallPath $ResolvedVsInstallPath -Architecture $Architecture

$VsCMake = Join-Path $ResolvedVsInstallPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$VsNinja = Join-Path $ResolvedVsInstallPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if (Test-Path -LiteralPath $VsCMake) {
    $env:XQ_CMAKE_EXE = (Resolve-Path -LiteralPath $VsCMake).Path
}
if (Test-Path -LiteralPath $VsNinja) {
    Add-XQPath -Path (Split-Path -Parent $VsNinja)
    $env:XQ_NINJA_EXE = (Resolve-Path -LiteralPath $VsNinja).Path
}

Write-Host "[Externals] Visual Studio: $ResolvedVsInstallPath"
