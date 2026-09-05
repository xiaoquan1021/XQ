Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'CMake\mitkInstallRules.cmake',
    'qwindowsvistastyle.dll',
    'qmodernwindowsstyle.dll'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must contain $required so install works with Qt 6.7 Windows style plugins"
    }
}
