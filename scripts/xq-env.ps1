param(
    [string]$XQRoot,
    [string]$BuildDir,
    [string]$ExternalsRoot,
    [string]$Platform
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-XQRepoRoot {
    param([string]$Root)

    if ($Root) {
        return [System.IO.Path]::GetFullPath($Root)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $MyInvocation.ScriptName) ".."))
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "xq-dotenv.ps1")
$RepoRootForDotEnv = Get-XQRepoRoot -Root $XQRoot
Import-XQDotEnv -RepoRoot $RepoRootForDotEnv | Out-Null
if (-not $BuildDir -and $env:XQ_BUILD_DIR) {
    $BuildDir = $env:XQ_BUILD_DIR
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

function Get-XQInstallRoot {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Platform
    )

    $platformInstall = Join-Path $Root (Join-Path "install" $Platform)
    if (Test-Path -LiteralPath $platformInstall) {
        return $platformInstall
    }
    return (Join-Path $Root "install")
}

function Get-XQMitkConfigCandidates {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$InstallRoot,
        [Parameter(Mandatory)][string]$Platform
    )

    @(
        (Join-Path $Root (Join-Path "build" (Join-Path $Platform "MITK\MITK-build\MITKConfig.cmake"))),
        (Join-Path $Root "src\MITK-2024.06\build\MITK-build\MITKConfig.cmake"),
        (Join-Path $InstallRoot "mitk-2024.06\MITKConfig.cmake"),
        (Join-Path $InstallRoot "mitk-2024.06\lib\cmake\MITK\MITKConfig.cmake")
    )
}

function Test-XQExternalsRoot {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Platform
    )

    $installRoot = Get-XQInstallRoot -Root $Root -Platform $Platform
    $required = @(
        (Join-Path $installRoot "qt-6.7.0\lib\cmake\Qt6\Qt6Config.cmake"),
        (Join-Path $installRoot "vtk-9.3.0\lib\cmake\vtk-9.3\vtk-config.cmake"),
        (Join-Path $installRoot "itk-5.4.0\lib\cmake\ITK-5.4\ITKConfig.cmake")
    )

    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path)) {
            return $false
        }
    }

    foreach ($path in Get-XQMitkConfigCandidates -Root $Root -InstallRoot $installRoot -Platform $Platform) {
        if (Test-Path -LiteralPath $path) {
            return $true
        }
    }

    return $false
}

function Resolve-XQExternalsRoot {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [string]$RequestedRoot,
        [Parameter(Mandatory)][string]$Platform
    )

    $candidates = @()
    if ($RequestedRoot) {
        $candidates += $RequestedRoot
    }
    elseif ($env:XQ_EXTERNALS_ROOT) {
        $candidates += $env:XQ_EXTERNALS_ROOT
    }
    else {
        $candidates += Join-Path $RepoRoot "..\Externals"
        $candidates += Join-Path $RepoRoot "..\svExternals"
        $candidates += Join-Path $RepoRoot "..\External"
        if ($env:USERPROFILE) {
            $candidates += Join-Path $env:USERPROFILE "Externals"
            $candidates += Join-Path $env:USERPROFILE "Simvascular\Externals"
        }
    }

    foreach ($candidate in $candidates) {
        if (-not $candidate) {
            continue
        }
        $resolved = [System.IO.Path]::GetFullPath($candidate)
        if (Test-XQExternalsRoot -Root $resolved -Platform $Platform) {
            return $resolved
        }
    }

    Write-Error "[XQ][ERROR] Missing required external dependency files." -ErrorAction Continue
    Write-Error "[XQ][ERROR] Checked candidate roots:" -ErrorAction Continue
    foreach ($candidate in $candidates) {
        Write-Error "  - $candidate" -ErrorAction Continue
    }
    Write-Error "[XQ][ERROR] Fetch sources with Externals/scripts/Fetch-Sources.ps1, build them with Externals/build_all.ps1, then set XQ_EXTERNALS_ROOT." -ErrorAction Continue
    throw "[XQ][ERROR] Unable to resolve XQ_EXTERNALS_ROOT."
}

function Add-XQPathEntry {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $entries = $env:PATH -split ";"
    if ($entries -notcontains $Path) {
        $env:PATH = "$Path;$env:PATH"
    }
}

function Initialize-XQEnvironment {
    param(
        [string]$Root,
        [string]$Build,
        [string]$ExternalRoot,
        [string]$ExternalPlatform = "windows-x64"
    )

    $repoRoot = Get-XQRepoRoot -Root $Root
    $buildRoot = if ($Build) { [System.IO.Path]::GetFullPath($Build) } else { Join-Path $repoRoot "build\windows-msvc-release" }
    $resolvedExternals = Resolve-XQExternalsRoot -RepoRoot $repoRoot -RequestedRoot $ExternalRoot -Platform $ExternalPlatform
    $installRoot = Get-XQInstallRoot -Root $resolvedExternals -Platform $ExternalPlatform

    $env:XQ_ROOT = $repoRoot
    $env:XQ_BUILD_DIR = $buildRoot
    $env:XQ_EXTERNALS_ROOT = $resolvedExternals
    $env:XQ_EXTERNALS_PLATFORM = $ExternalPlatform
    $env:XQ_EXTERNALS_INSTALL = $installRoot

    $runtimePaths = @(
        (Join-Path $buildRoot "bin"),
        (Join-Path $buildRoot "lib"),
        (Join-Path $buildRoot "lib\plugins"),
        (Join-Path $installRoot "qt-6.7.0\bin"),
        (Join-Path $installRoot "qt-6.7.0\plugins"),
        (Join-Path $installRoot "python-3.11.0"),
        (Join-Path $installRoot "hdf5-1.14.3\bin"),
        (Join-Path $installRoot "gdcm-3.0.10\bin"),
        (Join-Path $installRoot "vtk-9.3.0\bin"),
        (Join-Path $installRoot "itk-5.4.0\bin"),
        (Join-Path $installRoot "mitk-2024.06\bin"),
        (Join-Path $installRoot "opencascade-7.6.0\win64\vc14\bin"),
        (Join-Path $installRoot "opencascade-7.6.0\bin"),
        (Join-Path $installRoot "freetype-2.13.0\bin"),
        (Join-Path $resolvedExternals (Join-Path "build" (Join-Path $ExternalPlatform "MITK\MITK-build\bin"))),
        (Join-Path $resolvedExternals (Join-Path "build" (Join-Path $ExternalPlatform "MITK\MITK-build\bin\plugins"))),
        (Join-Path $resolvedExternals (Join-Path "build" (Join-Path $ExternalPlatform "MITK\ep\src\CTK-build\CTK-build\bin")))
    )

    foreach ($path in $runtimePaths) {
        Add-XQPathEntry -Path $path
    }

    $env:QT_PLUGIN_PATH = Join-Path $installRoot "qt-6.7.0\plugins"
    $env:XQ_PLUGIN_PATH = (Join-Path $buildRoot "lib\plugins")

    [pscustomobject]@{
        XQRoot = $repoRoot
        BuildDir = $buildRoot
        ExternalsRoot = $resolvedExternals
        ExternalsInstall = $installRoot
        Platform = $ExternalPlatform
        QtPluginPath = $env:QT_PLUGIN_PATH
        XQPluginPath = $env:XQ_PLUGIN_PATH
    }
}

Initialize-XQEnvironment -Root $XQRoot -Build $BuildDir -ExternalRoot $ExternalsRoot -ExternalPlatform $Platform
