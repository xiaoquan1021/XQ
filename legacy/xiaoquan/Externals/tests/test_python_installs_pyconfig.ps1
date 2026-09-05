Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$pythonBlock = [regex]::Match($Recipe, '(?s)"Python"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"HDF5"')
if (-not $pythonBlock.Success) {
    throw "Python recipe block not found"
}

$body = $pythonBlock.Groups["body"].Value
if ($body -notmatch 'PC\\pyconfig\.h') {
    throw "Python Windows install must copy PC\\pyconfig.h into include for CMake FindPython"
}
