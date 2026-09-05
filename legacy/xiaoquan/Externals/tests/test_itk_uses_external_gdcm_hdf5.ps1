Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$itkBlock = [regex]::Match($Recipe, '(?s)"ITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"OpenCascade"')
if (-not $itkBlock.Success) {
    throw "ITK recipe block not found"
}

$body = $itkBlock.Groups["body"].Value
foreach ($required in @(
    '"-DITK_USE_SYSTEM_GDCM=ON"',
    '"-DGDCM_DIR=$gdcmDir"',
    '"-DITK_USE_SYSTEM_HDF5=ON"',
    '"-DHDF5_DIR=$hdf5Dir"'
)) {
    if ($body -notmatch [regex]::Escape($required)) {
        throw "ITK recipe must contain $required"
    }
}

foreach ($variable in @('$gdcmDir', '$hdf5Dir')) {
    if ($body -notmatch ([regex]::Escape($variable) + '\s*=\s*ConvertTo-XQCMakePath')) {
        throw "ITK recipe must normalize $variable with ConvertTo-XQCMakePath"
    }
}
