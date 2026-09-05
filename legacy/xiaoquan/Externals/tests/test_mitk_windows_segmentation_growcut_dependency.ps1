Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'Modules\Segmentation\CMakeLists.txt',
    'ITK|QuadEdgeMesh+RegionGrowing+GrowCut'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must contain $required so Segmentation can include itkFastGrowCut.h"
    }
}
