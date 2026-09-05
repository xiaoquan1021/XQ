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
    '"-DMITK_ADDITIONAL_C_FLAGS=/utf-8"',
    '"-DMITK_ADDITIONAL_CXX_FLAGS=/utf-8"'
)) {
    if ($body -notmatch [regex]::Escape($required)) {
        throw "MITK Windows/MSVC recipe must contain $required so C4819 is not promoted to an error by /WX"
    }
}
