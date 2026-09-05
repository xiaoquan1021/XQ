Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Recipe -notmatch 'function\s+Update-XQMitkToolkitSource') {
    throw "MITK recipe must provide a reproducible source patch for the XQ toolkit build"
}

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
foreach ($required in @(
    'Update-XQMitkToolkitSource -SourceDir $SourceDir',
    '"-DMITK_WHITELIST=XQToolkit"'
)) {
    if ($body -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit recipe must contain $required"
    }
}

foreach ($required in @(
    'Chart',
    'AppUtil',
    'ImageStatisticsUI',
    'WebEngineCore',
    'WebEngineWidgets'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must explicitly account for $required"
    }
}

