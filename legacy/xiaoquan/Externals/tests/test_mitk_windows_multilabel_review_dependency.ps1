Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Recipe -notmatch 'function\s+Update-XQMitkToolkitSource') {
    throw "MITK recipe must provide a reproducible source patch for the XQ toolkit build"
}

foreach ($required in @(
    'Modules\Multilabel\CMakeLists.txt',
    'ITK|Smoothing+Review'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must contain $required so Multilabel can include itkLabelGeometryImageFilter.h"
    }
}
