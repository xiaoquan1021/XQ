Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ExternalsCMake = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\CMake\XQExternals.cmake")

foreach ($required in @(
    'MITK_EXTERNAL_PROJECT_PREFIX',
    '${MITK_DIR}/../ep',
    'DCMQI_DIR'
)) {
    if ($ExternalsCMake -notmatch [regex]::Escape($required)) {
        throw "XQ externals must expose MITK superbuild ep prefix for build-tree MITK dependencies"
    }
}
