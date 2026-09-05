Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body -notmatch 'New-XQShortDirectoryJunction') {
    throw "MITK recipe must use short junction paths before invoking CMake on Windows"
}

if ($body -notmatch 'mitk-src' -or $body -notmatch 'mitk-bld') {
    throw "MITK recipe must use stable short source/build aliases below C:\xq-ext"
}

if ($body -notmatch '-SourceDir\s+\$mitkSourceDir' -or $body -notmatch '-BuildDir\s+\$mitkBuildDir') {
    throw "MITK CMake invocation must pass the short source and build aliases"
}
