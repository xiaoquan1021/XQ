[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$LegacyQtRoot,
    [Parameter(Mandatory)][string]$VtkRoot,
    [Parameter(Mandatory)][string]$ItkRoot,
    [Parameter(Mandatory)][string]$GdcmRoot,
    [Parameter(Mandatory)][string]$Hdf5Root,
    [Parameter(Mandatory)][string]$PythonRoot,
    [Parameter(Mandatory)][string]$TinyXml2Root,
    [Parameter(Mandatory)][string]$Dumpbin,
    [string]$MmgRoot
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
        throw "$Label resolved outside the locked prefix: $actualFull != $expectedFull"
    }
}

function Get-CMakeSetPath {
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Source
    )

    $pattern = '(?m)^\s*set\({0}\s+"([^"]+)"\s*\)' -f [regex]::Escape($Name)
    $match = [regex]::Match($Text, $pattern)
    if (-not $match.Success) {
        throw "$Source does not define $Name with an explicit package path"
    }
    return $match.Groups[1].Value
}

$BuildDir = Resolve-RequiredPath -Path $BuildDir -Label "Build directory"
$QtRoot = Resolve-RequiredPath -Path $QtRoot -Label "Qt root"
$LegacyQtRoot = Resolve-RequiredPath -Path $LegacyQtRoot -Label "Legacy Qt root"
$VtkRoot = Resolve-RequiredPath -Path $VtkRoot -Label "VTK root"
$ItkRoot = Resolve-RequiredPath -Path $ItkRoot -Label "ITK root"
$GdcmRoot = Resolve-RequiredPath -Path $GdcmRoot -Label "GDCM root"
$Hdf5Root = Resolve-RequiredPath -Path $Hdf5Root -Label "HDF5 root"
$PythonRoot = Resolve-RequiredPath -Path $PythonRoot -Label "Configure-only Python root"
$TinyXml2Root = Resolve-RequiredPath -Path $TinyXml2Root -Label "TinyXML2 root"
$Dumpbin = Resolve-RequiredPath -Path $Dumpbin -Label "dumpbin"
if (-not [string]::IsNullOrWhiteSpace($MmgRoot)) {
    $MmgRoot = Resolve-RequiredPath -Path $MmgRoot -Label "MMG root"
}

$cachePath = Resolve-RequiredPath -Path (Join-Path $BuildDir "CMakeCache.txt") -Label "CMake cache"
$cacheText = Get-Content -Raw -LiteralPath $cachePath
if ((Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_BUILD_TYPE") -ne "Release") {
    throw "Dependency baseline must be configured in Release mode"
}
if ((Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_FIND_USE_PACKAGE_REGISTRY") -ne "FALSE") {
    throw "CMake user package registry must be disabled"
}
if ((Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY") -ne "FALSE") {
    throw "CMake system package registry must be disabled"
}

Assert-SamePath -Label "Qt6_DIR" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "Qt6_DIR") `
    -Expected (Join-Path $QtRoot "lib\cmake\Qt6")
Assert-SamePath -Label "VTK_DIR" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "VTK_DIR") `
    -Expected (Join-Path $VtkRoot "lib\cmake\vtk-9.3")
Assert-SamePath -Label "ITK_DIR" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "ITK_DIR") `
    -Expected (Join-Path $ItkRoot "lib\cmake\ITK-5.4")
Assert-SamePath -Label "tinyxml2_DIR" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "tinyxml2_DIR") `
    -Expected (Join-Path $TinyXml2Root "lib\cmake\tinyxml2")

$prefixEntries = @(
    (Get-CMakeCacheValue -CacheText $cacheText -Name "CMAKE_PREFIX_PATH") -split ';' |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)
$expectedPrefixEntries = @(
    $QtRoot,
    $GdcmRoot,
    $Hdf5Root,
    $ItkRoot,
    $TinyXml2Root,
    $VtkRoot
)
if ($prefixEntries.Count -ne $expectedPrefixEntries.Count) {
    throw "CMAKE_PREFIX_PATH has $($prefixEntries.Count) entries; expected the six locked roots"
}
for ($index = 0; $index -lt $expectedPrefixEntries.Count; ++$index) {
    Assert-SamePath -Label "CMAKE_PREFIX_PATH[$index]" `
        -Actual $prefixEntries[$index] `
        -Expected $expectedPrefixEntries[$index]
}

$itkDir = Get-CMakeCacheValue -CacheText $cacheText -Name "ITK_DIR"
$itkGdcmModule = Resolve-RequiredPath `
    -Path (Join-Path $itkDir "Modules\ITKGDCM.cmake") `
    -Label "ITK GDCM module metadata"
$itkHdf5Module = Resolve-RequiredPath `
    -Path (Join-Path $itkDir "Modules\ITKHDF5.cmake") `
    -Label "ITK HDF5 module metadata"
$itkGdcmText = Get-Content -Raw -LiteralPath $itkGdcmModule
$itkHdf5Text = Get-Content -Raw -LiteralPath $itkHdf5Module
Assert-SamePath -Label "ITK embedded GDCM_DIR" `
    -Actual (Get-CMakeSetPath -Text $itkGdcmText -Name "GDCM_DIR" -Source $itkGdcmModule) `
    -Expected (Join-Path $GdcmRoot "lib\gdcm-3.0")
Assert-SamePath -Label "ITK embedded HDF5_DIR" `
    -Actual (Get-CMakeSetPath -Text $itkHdf5Text -Name "HDF5_DIR" -Source $itkHdf5Module) `
    -Expected (Join-Path $Hdf5Root "cmake")
Resolve-RequiredPath -Path (Join-Path $GdcmRoot "lib\gdcm-3.0\GDCMConfig.cmake") `
    -Label "GDCM package config" | Out-Null
Resolve-RequiredPath -Path (Join-Path $Hdf5Root "cmake\hdf5-config.cmake") `
    -Label "HDF5 package config" | Out-Null
$gdcmVersionPath = Resolve-RequiredPath `
    -Path (Join-Path $GdcmRoot "lib\gdcm-3.0\GDCMConfigVersion.cmake") `
    -Label "GDCM package version metadata"
$hdf5VersionPath = Resolve-RequiredPath `
    -Path (Join-Path $Hdf5Root "cmake\hdf5-config-version.cmake") `
    -Label "HDF5 package version metadata"
$gdcmVersionText = Get-Content -Raw -LiteralPath $gdcmVersionPath
$hdf5VersionText = Get-Content -Raw -LiteralPath $hdf5VersionPath
if ($gdcmVersionText -notmatch '(?m)^\s*set\(\s*GDCM_VERSION\s+"3\.0\.10"\s*\)') {
    throw "GDCM package metadata does not identify exact version 3.0.10: $gdcmVersionPath"
}
if ($hdf5VersionText -notmatch '(?m)^\s*set\(PACKAGE_VERSION\s+"1\.14\.3"\s*\)') {
    throw "HDF5 package metadata does not identify exact version 1.14.3: $hdf5VersionPath"
}

Assert-SamePath -Label "configure-only Python executable" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "_Python3_EXECUTABLE") `
    -Expected (Join-Path $PythonRoot "python.exe")
Assert-SamePath -Label "configure-only Python include" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "_Python3_INCLUDE_DIR") `
    -Expected (Join-Path $PythonRoot "include")
Assert-SamePath -Label "configure-only Python library" `
    -Actual (Get-CMakeCacheValue -CacheText $cacheText -Name "_Python3_LIBRARY_RELEASE") `
    -Expected (Join-Path $PythonRoot "libs\python311.lib")
if ($cacheText -match '(?i)C:[\\/]software[\\/]anaconda') {
    throw "CMake cache contains a host Anaconda path: $($Matches[0])"
}

$graphFiles = @()
$buildNinja = Join-Path $BuildDir "build.ninja"
if (Test-Path -LiteralPath $buildNinja) {
    $graphFiles += $buildNinja
}
$graphFiles += Get-ChildItem -LiteralPath $BuildDir -Recurse -File |
    Where-Object { $_.Name -eq "link.txt" -or $_.Extension -eq ".rsp" } |
    Select-Object -ExpandProperty FullName
if ($graphFiles.Count -eq 0) {
    throw "No generated build graph files were found under $BuildDir"
}

$graphText = ($graphFiles | ForEach-Object { Get-Content -Raw -LiteralPath $_ }) -join "`n"
$forbiddenGraphPatterns = [ordered]@{
    "host Anaconda path" = '(?i)C:[\\/]software[\\/]anaconda'
    "Python import library" = '(?i)python[0-9.]*\.lib'
    "VTK Python target" = '(?i)vtk(?:WrappingPython|PythonInterpreter|PythonContext2D|CommonPython|FiltersPython)'
    "MITK/Slicer/CTK/BlueBerry path" = '(?i)[\\/](?:MITK|Slicer|CTK|BlueBerry)(?:[\\/]|\.)'
    "legacy Qt prefix" = '(?i)install[\\/]+windows-x64[\\/]+qt-6\.7\.0'
}
foreach ($entry in $forbiddenGraphPatterns.GetEnumerator()) {
    if ($graphText -match $entry.Value) {
        throw "Generated build graph contains forbidden $($entry.Key): $($Matches[0])"
    }
}

$entryPoints = @(
    "xq_app.exe",
    "test_itk_vascular_segmenter.exe",
    "test_dicom_series_adapter.exe",
    "test_main_window.exe"
)
if (-not [string]::IsNullOrWhiteSpace($MmgRoot)) {
    $entryPoints += @("test_tetgen_volume_mesh.exe", "test_mmg_volume_mesh.exe")
}
$runtimeRoots = @(
    $BuildDir,
    (Join-Path $QtRoot "bin"),
    (Join-Path $VtkRoot "bin"),
    (Join-Path $ItkRoot "bin"),
    (Join-Path $GdcmRoot "bin"),
    (Join-Path $Hdf5Root "bin"),
    (Join-Path $TinyXml2Root "bin"),
    (Join-Path $env:SystemRoot "System32"),
    (Join-Path $env:SystemRoot "SysWOW64")
)
if (-not [string]::IsNullOrWhiteSpace($MmgRoot)) {
    $runtimeRoots += (Join-Path $MmgRoot "bin")
}
$runtimeRoots = $runtimeRoots | Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object {
    (Resolve-Path -LiteralPath $_).Path
} | Select-Object -Unique

$systemRoot = [IO.Path]::GetFullPath($env:SystemRoot).TrimEnd('\', '/')
$queue = [Collections.Generic.Queue[string]]::new()
foreach ($entryPoint in $entryPoints) {
    $queue.Enqueue((Resolve-RequiredPath -Path (Join-Path $BuildDir $entryPoint) -Label $entryPoint))
}

$visited = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$forbiddenDll = '(?i)(python|mitk|slicer|ctk|blueberry|zstd|vmtk)'
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
            throw "Unresolved runtime dependency $dependency imported by $binary"
        }

        $resolvedFull = [IO.Path]::GetFullPath($resolved)
        if (-not $resolvedFull.StartsWith($systemRoot, [StringComparison]::OrdinalIgnoreCase)) {
            $queue.Enqueue($resolved)
        }
    }
}

Write-Host "[XQ dependency baseline] build graph clean; PE binaries scanned: $($visited.Count)"
