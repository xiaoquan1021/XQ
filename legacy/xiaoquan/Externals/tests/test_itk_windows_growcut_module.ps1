Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$itkBlock = [regex]::Match($Recipe, '(?s)"ITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"OpenCascade"')
if (-not $itkBlock.Success) {
    throw "ITK recipe block not found"
}

$body = $itkBlock.Groups["body"].Value
$required = '"-DModule_GrowCut=ON"'
if ($body -notmatch [regex]::Escape($required)) {
    throw "ITK Windows recipe must contain $required because MITK Segmentation includes itkFastGrowCut.h"
}
