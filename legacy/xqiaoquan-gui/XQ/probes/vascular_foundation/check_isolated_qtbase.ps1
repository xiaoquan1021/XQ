[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$LegacyQtRoot,
    [Parameter(Mandatory)][string]$Dumpbin,
    [string]$ExpectedLegacyQtCoreSha256 = "35DEE962158C90E7958F3540070C3E6013A140401D2479D05378097FE11DB629"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RequiredPath {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
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

$BuildDir = Resolve-RequiredPath -Path $BuildDir -Label "QtBase build directory"
$QtRoot = Resolve-RequiredPath -Path $QtRoot -Label "QtBase install root"
$LegacyQtRoot = Resolve-RequiredPath -Path $LegacyQtRoot -Label "Legacy Qt root"
$Dumpbin = Resolve-RequiredPath -Path $Dumpbin -Label "dumpbin"

$cachePath = Resolve-RequiredPath -Path (Join-Path $BuildDir "CMakeCache.txt") -Label "QtBase CMake cache"
$cacheText = Get-Content -Raw -LiteralPath $cachePath
Assert-SamePath -Label "QtBase install prefix" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_INSTALL_PREFIX") `
    -Expected $QtRoot
if ((Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_BUILD_TYPE") -ne "Release") {
    throw "QtBase must be built in Release mode"
}
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
$zstdDirMatch = [regex]::Match($cacheText, '(?m)^zstd_DIR:[^=]*=(.*)$')
if ($zstdDirMatch.Success -and
        $zstdDirMatch.Groups[1].Value.Trim() -notmatch '(?i)(^$|-NOTFOUND$)') {
    throw "QtBase cache resolved an external zstd package: $($zstdDirMatch.Groups[1].Value.Trim())"
}

$versionFiles = Get-ChildItem -LiteralPath (Join-Path $QtRoot "lib\cmake\Qt6") `
    -Filter "Qt6ConfigVersion*.cmake" -File
if (-not $versionFiles -or
        -not (($versionFiles | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }) -join "`n" -match '6\.7\.0')) {
    throw "Installed Qt package metadata does not identify Qt 6.7.0"
}

$legacyCore = Resolve-RequiredPath -Path (Join-Path $LegacyQtRoot "bin\Qt6Core.dll") -Label "Legacy Qt6Core.dll"
if (-not [string]::IsNullOrWhiteSpace($ExpectedLegacyQtCoreSha256)) {
    $legacyHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $legacyCore).Hash
    if (-not $legacyHash.Equals($ExpectedLegacyQtCoreSha256, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Legacy Qt6Core.dll changed: $legacyHash != $ExpectedLegacyQtCoreSha256"
    }
}

$entryPoints = @(
    (Join-Path $QtRoot "bin\Qt6Core.dll"),
    (Join-Path $QtRoot "bin\Qt6Gui.dll"),
    (Join-Path $QtRoot "bin\Qt6Widgets.dll"),
    (Join-Path $QtRoot "bin\Qt6OpenGL.dll"),
    (Join-Path $QtRoot "bin\Qt6OpenGLWidgets.dll"),
    (Join-Path $QtRoot "plugins\platforms\qwindows.dll"),
    (Join-Path $QtRoot "plugins\platforms\qoffscreen.dll")
) | ForEach-Object { Resolve-RequiredPath -Path $_ -Label (Split-Path -Leaf $_) }

$runtimeRoots = @(
    (Join-Path $QtRoot "bin"),
    (Join-Path $env:SystemRoot "System32"),
    (Join-Path $env:SystemRoot "SysWOW64")
) | Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object {
    (Resolve-Path -LiteralPath $_).Path
}
$qtRootFull = [IO.Path]::GetFullPath($QtRoot).TrimEnd('\', '/')
$systemRootFull = [IO.Path]::GetFullPath($env:SystemRoot).TrimEnd('\', '/')
$queue = [Collections.Generic.Queue[string]]::new()
foreach ($entryPoint in $entryPoints) {
    $queue.Enqueue($entryPoint)
}
$visited = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$forbiddenDll = '(?i)(zstd|python|mitk|slicer|ctk|blueberry|vmtk)'
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $visited.Add($binary)) {
        continue
    }

    $output = & $Dumpbin /dependents $binary 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed for $binary"
    }
    foreach ($line in $output) {
        if ($line -notmatch '^\s*([A-Za-z0-9_.+-]+\.dll)\s*$') {
            continue
        }
        $dependency = $Matches[1]
        if ($dependency -match $forbiddenDll) {
            throw "Forbidden runtime dependency $dependency imported by $binary"
        }
        if ($dependency -match '^(?i:api-ms-win|ext-ms-win)-') {
            continue
        }

        $resolved = $null
        foreach ($root in $runtimeRoots) {
            $candidate = Join-Path $root $dependency
            if (Test-Path -LiteralPath $candidate) {
                $resolved = (Resolve-Path -LiteralPath $candidate).Path
                break
            }
        }
        if (-not $resolved) {
            throw "Unresolved Qt runtime dependency $dependency imported by $binary"
        }

        $resolvedFull = [IO.Path]::GetFullPath($resolved)
        if (-not $resolvedFull.StartsWith($systemRootFull, [StringComparison]::OrdinalIgnoreCase)) {
            if (-not $resolvedFull.StartsWith($qtRootFull, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Qt runtime dependency resolved outside the isolated prefix: $resolvedFull"
            }
            $queue.Enqueue($resolved)
        }
    }
}

$coreHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $QtRoot "bin\Qt6Core.dll")).Hash
Write-Host "[XQ Qt baseline] Qt 6.7.0 cache/import closure clean; binaries scanned: $($visited.Count); Qt6Core SHA256=$coreHash"
