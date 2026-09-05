Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Helpers = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Helpers.ps1")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Helpers -notmatch 'function\s+New-XQShortDirectoryJunction') {
    throw "Build helpers must provide a short directory junction helper for dependencies with Windows path-length guards"
}

$itkBlock = [regex]::Match($Recipe, '(?s)"ITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"OpenCascade"')
if (-not $itkBlock.Success) {
    throw "ITK recipe block not found"
}

$body = $itkBlock.Groups["body"].Value
if ($body -notmatch 'New-XQShortDirectoryJunction') {
    throw "ITK recipe must use short junction paths before invoking CMake on Windows"
}

if ($body -notmatch 'itk-src' -or $body -notmatch 'itk-bld') {
    throw "ITK recipe must use stable short source/build aliases below C:\xq-ext"
}

if ($body -notmatch '-SourceDir\s+\$itkSourceDir' -or $body -notmatch '-BuildDir\s+\$itkBuildDir') {
    throw "ITK CMake invocation must pass the short source and build aliases"
}
