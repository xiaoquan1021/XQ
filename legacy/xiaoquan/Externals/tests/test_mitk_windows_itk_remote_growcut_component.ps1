Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'CMake\PackageDepends\MITK_ITK_Config.cmake',
    'STREQUAL "GrowCut"'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must contain $required so the ITK remote GrowCut component is not renamed to ITKGrowCut"
    }
}
