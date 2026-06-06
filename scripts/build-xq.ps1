param(
    [string[]]$Command = @("configure", "build", "test"),
    [string]$BuildDir,
    [string]$BuildType = "Release",
    [string]$ExternalsRoot,
    [string]$Platform = "windows-x64",
    [string]$VsInstallPath,
    [string]$CMake,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir ".."))

function Show-Usage {
    @"
Usage: powershell -File scripts/build-xq.ps1 [commands]

Commands:
  doctor      Print resolved paths and verify required external dependencies.
  configure   Configure CMake with the windows-msvc-release preset.
  build       Build the configured tree.
  test        Run ctest for the configured tree.
  clean       Remove the Windows build directory.

Options:
  -ExternalsRoot <path>   Root of the Externals entry repository.
  -BuildDir <path>        Build directory. Defaults to build/windows-msvc-release.
  -VsInstallPath <path>   Visual Studio 2022 installation path.
  -CMake <path>           CMake executable.
"@
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

    $vsCMake = Join-Path $VsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $vsCMake) {
        return (Resolve-Path -LiteralPath $vsCMake).Path
    }

    throw "[XQ][ERROR] CMake was not found."
}

if ($Help) {
    Show-Usage
    exit 0
}

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build\windows-msvc-release"
}

$VsPath = Find-XQVisualStudioInstall -PreferredPath $VsInstallPath
Import-XQVisualStudioEnvironment -InstallPath $VsPath
$CMakeExe = Resolve-XQCMake -RequestedCMake $CMake -VsPath $VsPath

function Invoke-XQEnvScript {
    $envScript = Join-Path $ScriptDir "xq-env.ps1"
    $envParams = @{
        XQRoot = $RepoRoot
        BuildDir = $BuildDir
        Platform = $Platform
    }
    if ($ExternalsRoot) {
        $envParams.ExternalsRoot = $ExternalsRoot
    }

    . $envScript @envParams
}

foreach ($item in $Command) {
    switch ($item) {
        "clean" {
            if (Test-Path -LiteralPath $BuildDir) {
                Remove-Item -Recurse -Force -LiteralPath $BuildDir
            }
        }
        "doctor" {
            Invoke-XQEnvScript
            Write-Host "[XQ] Visual Studio: $VsPath"
            Write-Host "[XQ] CMake: $CMakeExe"
        }
        "configure" {
            $configureArgs = @(
                "--preset", "windows-msvc-release",
                "-DXQ_EXTERNALS_PLATFORM=$Platform",
                "-DCMAKE_BUILD_TYPE=$BuildType"
            )
            if ($ExternalsRoot) {
                $configureArgs += "-DXQ_EXTERNALS_ROOT=$ExternalsRoot"
            }
            & $CMakeExe @configureArgs
            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }
        "build" {
            & $CMakeExe --build $BuildDir --config $BuildType
            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }
        "test" {
            Invoke-XQEnvScript | Out-Null
            & $CMakeExe --build $BuildDir --target test --config $BuildType
            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }
        default {
            throw "[XQ][ERROR] Unknown command: $item"
        }
    }
}
