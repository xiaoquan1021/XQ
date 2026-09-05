[CmdletBinding()]
param(
    [string]$ExternalsRoot,
    [string]$Platform = "windows-x64-vascular",
    [string]$BuildDir,
    [string]$InstallDir,
    [ValidateRange(1, 64)][int]$Jobs = 8,
    [string]$VsInstallPath = "C:\software\Visual Studio\Visual Studio2022\Community",
    [switch]$ConfigureOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RequiredDirectory {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Label does not exist: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory)][string]$CacheText,
        [Parameter(Mandatory)][string]$Name
    )

    $match = [regex]::Match($CacheText, "(?m)^$([regex]::Escape($Name)):[^=]*=(.*)$")
    if (-not $match.Success) {
        throw "CMake cache entry is missing: $Name"
    }
    return $match.Groups[1].Value.Trim()
}

function Assert-SamePath {
    param(
        [Parameter(Mandatory)][string]$Actual,
        [Parameter(Mandatory)][string]$Expected,
        [Parameter(Mandatory)][string]$Label
    )

    $actualFull = [IO.Path]::GetFullPath($Actual).TrimEnd('\', '/')
    $expectedFull = [IO.Path]::GetFullPath($Expected).TrimEnd('\', '/')
    if (-not $actualFull.Equals($expectedFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label mismatch: $actualFull != $expectedFull"
    }
}

$probeScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $probeScriptDir "..\..\..")).Path
if ([string]::IsNullOrWhiteSpace($ExternalsRoot)) {
    $ExternalsRoot = Join-Path $repoRoot "..\XIAOQUAN\Externals"
}
$ExternalsRoot = Resolve-RequiredDirectory -Path $ExternalsRoot -Label "Externals root"

$sourceDir = Resolve-RequiredDirectory `
    -Path (Join-Path $ExternalsRoot "src\qt-everywhere-src-6.7.0") `
    -Label "Qt 6.7.0 source"
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $ExternalsRoot "build\$Platform\QtBaseClean"
}
if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    $InstallDir = Join-Path $ExternalsRoot "install\$Platform\qt-6.7.0"
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$InstallDir = [IO.Path]::GetFullPath($InstallDir)

$configure = Join-Path $sourceDir "configure.bat"
$vsInit = Join-Path $ExternalsRoot "scripts\Initialize-VS2022.ps1"
foreach ($required in @($configure, $vsInit)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required build input is missing: $required"
    }
}

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if ((Test-Path -LiteralPath $InstallDir) -and
        -not (Test-Path -LiteralPath $cachePath) -and
        (Get-ChildItem -LiteralPath $InstallDir -Force | Select-Object -First 1)) {
    throw "Refusing to overwrite a non-empty Qt install without its matching build cache: $InstallDir"
}
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

# Reuse the repository's proven VS environment importer, then remove host
# package-manager paths before Qt performs feature/package discovery.
. $vsInit -VsInstallPath $VsInstallPath -Architecture x64
$blockedPath = '(?i)(anaconda|miniconda|[\\/]Qt[\\/]|[\\/]VTK[\\/]|[\\/]ITK[\\/]|mingw|vcpkg|MATLAB|WindowsApps)'
$candidatePath = @(
    (Split-Path -Parent $env:XQ_CMAKE_EXE),
    (Split-Path -Parent $env:XQ_NINJA_EXE)
) + ($env:Path -split ';')
$seenPath = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$cleanPath = [Collections.Generic.List[string]]::new()
foreach ($entry in $candidatePath) {
    $trimmed = $entry.Trim().TrimEnd('\', '/')
    if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed -match $blockedPath) {
        continue
    }
    if ($seenPath.Add($trimmed)) {
        $cleanPath.Add($trimmed)
    }
}
$env:Path = $cleanPath -join ';'

foreach ($name in @(
        "CMAKE_PREFIX_PATH",
        "CMAKE_MODULE_PATH",
        "CMAKE_TOOLCHAIN_FILE",
        "Qt6_DIR",
        "Qt5_DIR",
        "zstd_DIR",
        "ZSTD_ROOT",
        "TIFF_DIR",
        "WebP_DIR",
        "Python3_ROOT_DIR",
        "Python3_EXECUTABLE",
        "VCPKG_ROOT")) {
    Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
}
Get-ChildItem Env: |
    Where-Object { $_.Name -match '^(CONDA|PYTHON|PIP)' } |
    ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" -ErrorAction SilentlyContinue }

$env:VSLANG = "1033"
$env:CMAKE_GENERATOR = "Ninja"
$cmake = (Resolve-Path -LiteralPath $env:XQ_CMAKE_EXE).Path
$ninja = (Resolve-Path -LiteralPath $env:XQ_NINJA_EXE).Path

if (-not (Test-Path -LiteralPath $cachePath)) {
    Write-Host "[XQ Qt baseline] configuring isolated QtBase 6.7.0"
    Write-Host "[XQ Qt baseline] source:  $sourceDir"
    Write-Host "[XQ Qt baseline] build:   $BuildDir"
    Write-Host "[XQ Qt baseline] install: $InstallDir"
    Push-Location $BuildDir
    try {
        & $configure `
            -prefix $InstallDir `
            -opensource `
            -confirm-license `
            -release `
            -nomake examples `
            -nomake tests `
            -no-feature-zstd `
            -submodules qtbase `
            -- `
            "-DCMAKE_MAKE_PROGRAM=$ninja" `
            -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON `
            -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF `
            -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
        if ($LASTEXITCODE -ne 0) {
            throw "QtBase configure failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

$cacheText = Get-Content -Raw -LiteralPath $cachePath
Assert-SamePath -Label "CMAKE_HOME_DIRECTORY" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_HOME_DIRECTORY") `
    -Expected $sourceDir
Assert-SamePath -Label "CMAKE_INSTALL_PREFIX" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_INSTALL_PREFIX") `
    -Expected $InstallDir
if ((Get-CMakeCacheValue -CacheText $cacheText -Name "FEATURE_zstd") -ne "OFF" -or
        (Get-CMakeCacheValue -CacheText $cacheText -Name "QT_FEATURE_zstd") -ne "OFF") {
    throw "QtBase cache did not disable zstd"
}
if ((Get-CMakeCacheValue -CacheText $cacheText -Name "QT_BUILD_SUBMODULES") -ne "qtbase") {
    throw "Qt build is not restricted to the qtbase submodule"
}
if ($cacheText -match '(?i)C:[\\/]software[\\/]anaconda') {
    throw "QtBase cache contains a host Anaconda path: $($Matches[0])"
}

if ($ConfigureOnly) {
    Write-Host "[XQ Qt baseline] configure-only validation passed"
    exit 0
}

& $cmake --build $BuildDir --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    throw "QtBase build failed with exit code $LASTEXITCODE"
}
& $cmake --install $BuildDir
if ($LASTEXITCODE -ne 0) {
    throw "QtBase install failed with exit code $LASTEXITCODE"
}

Write-Host "[XQ Qt baseline] isolated QtBase install completed: $InstallDir"
