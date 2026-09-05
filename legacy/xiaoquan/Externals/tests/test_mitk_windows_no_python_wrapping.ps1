Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
foreach ($required in @(
    '"-DMITK_USE_Python3=OFF"',
    '"-DMITK_USE_SWIG=OFF"'
)) {
    if ($body -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit recipe must contain $required to avoid an undeclared NumPy source dependency"
    }
}

if ($body -match '"-DMITK_USE_Python3=1"' -or $body -match '"-DMITK_USE_SWIG=1"') {
    throw "MITK toolkit recipe must not enable MITK Python wrapping"
}
