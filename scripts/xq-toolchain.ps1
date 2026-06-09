Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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

    foreach ($root in @(
        "C:\software\Visual Studio\Visual Studio2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
    )) {
        if (Test-Path -LiteralPath $root) {
            return (Resolve-Path -LiteralPath $root).Path
        }
    }

    throw "[XQ][ERROR] Visual Studio 2022 with MSVC x64 tools was not found."
}

function Import-XQVisualStudioEnvironment {
    param([Parameter(Mandatory)][string]$InstallPath)

    $vsDevCmd = Join-Path $InstallPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path -LiteralPath $vsDevCmd)) {
        throw "[XQ][ERROR] VsDevCmd.bat not found under $InstallPath"
    }

    $cmd = "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && set"
    $envLines = & cmd.exe /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "[XQ][ERROR] Failed to initialize Visual Studio environment."
    }

    foreach ($line in $envLines) {
        $idx = $line.IndexOf("=")
        if ($idx -le 0) {
            continue
        }
        [Environment]::SetEnvironmentVariable(
            $line.Substring(0, $idx),
            $line.Substring($idx + 1),
            "Process")
    }
}

function Resolve-XQCMake {
    param([string]$RequestedCMake, [string]$VsPath)

    if ($RequestedCMake -and (Test-Path -LiteralPath $RequestedCMake)) {
        return (Resolve-Path -LiteralPath $RequestedCMake).Path
    }

    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    if ($VsPath) {
        $vsCMake = Join-Path $VsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path -LiteralPath $vsCMake) {
            return (Resolve-Path -LiteralPath $vsCMake).Path
        }
    }

    throw "[XQ][ERROR] CMake was not found."
}
