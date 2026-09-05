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
    '"-DMITK_BUILD_CONFIGURATION=Custom"',
    '"-DMITK_USE_BLUEBERRY=OFF"',
    '"-DBLUEBERRY_USE_QT_HELP=OFF"',
    '"-DBLUEBERRY_QT_HELP_REQUIRED=OFF"',
    '"-DMITK_BUILD_ALL_APPS=OFF"',
    '"-DMITK_USE_Qt6=ON"'
)) {
    if ($body -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit recipe must contain $required"
    }
}

if ($body -match 'WorkbenchRelease') {
    throw "MITK toolkit recipe must not use WorkbenchRelease because it requires Doxygen and BlueBerry help"
}
