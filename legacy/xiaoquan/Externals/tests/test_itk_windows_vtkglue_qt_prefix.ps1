Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$itkBlock = [regex]::Match($Recipe, '(?s)"ITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"OpenCascade"')
if (-not $itkBlock.Success) {
    throw "ITK recipe block not found"
}

$body = $itkBlock.Groups["body"].Value
if ($body -notmatch 'Get-XQExternalInstallPath -Name "Qt"') {
    throw "ITK VtkGlue recipe must resolve the Qt install root because VTK's CMake package requires Qt6"
}

if ($body -notmatch '"-DQt6_DIR=\$qt6Dir"') {
    throw "ITK VtkGlue recipe must pass Qt6_DIR for VTK's find_package(Qt6)"
}

if ($body -notmatch '"-DCMAKE_PREFIX_PATH=\$qtCmakeDir') {
    throw "ITK VtkGlue recipe must include the Qt CMake prefix path"
}
