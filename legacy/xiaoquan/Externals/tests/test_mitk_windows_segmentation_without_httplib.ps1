Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Recipe -notmatch 'function\s+Update-XQMitkToolkitSource') {
    throw "MITK recipe must provide a reproducible source patch for the XQ toolkit build"
}

foreach ($required in @(
    'Modules\Segmentation\CMakeLists.txt',
    'Modules\Segmentation\files.cmake',
    'mitkMonaiLabelTool.cpp',
    'mitkMonaiLabel2DTool.cpp',
    'mitkMonaiLabel3DTool.cpp',
    'PUBLIC ITK|QuadEdgeMesh+RegionGrowing httplib'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must explicitly remove Segmentation httplib/MONAI dependency: $required"
    }
}

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body -notmatch [regex]::Escape('"-DMITK_USE_httplib=OFF"')) {
    throw "MITK toolkit recipe should keep httplib disabled after removing MONAI Label sources"
}

