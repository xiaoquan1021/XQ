param(
    [string[]]$Command = @("configure", "build", "test"),
    [string]$BuildDir,
    [string]$BuildType = "Release",
    [string]$ExternalsRoot,
    [string]$Platform,
    [string]$VsInstallPath,
    [string]$CMake,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir ".."))
. (Join-Path $ScriptDir "xq-dotenv.ps1")
. (Join-Path $ScriptDir "xq-toolchain.ps1")
Import-XQDotEnv -RepoRoot $RepoRoot | Out-Null

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
  .env                    Optional local environment file copied from .env.example.
"@
}

if ($Help) {
    Show-Usage
    exit 0
}

if (-not $BuildDir -and $env:XQ_BUILD_DIR) {
    $BuildDir = $env:XQ_BUILD_DIR
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build\windows-msvc-release"
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
if (-not $VsInstallPath -and $env:XQ_VS_INSTALL_PATH) {
    $VsInstallPath = $env:XQ_VS_INSTALL_PATH
}
if (-not $CMake -and $env:XQ_CMAKE) {
    $CMake = $env:XQ_CMAKE
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
