Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ExternalsCMake = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\CMake\XQExternals.cmake")

if ($ExternalsCMake -notmatch [regex]::Escape('opencascade-7.6.0/cmake')) {
    throw "XQ externals must use the OpenCascade Windows install config directory"
}

if ($ExternalsCMake -match [regex]::Escape('opencascade-7.6.0/lib/cmake/opencascade')) {
    throw "XQ externals must not point OpenCascade to the incomplete build-tree fallback layout"
}
