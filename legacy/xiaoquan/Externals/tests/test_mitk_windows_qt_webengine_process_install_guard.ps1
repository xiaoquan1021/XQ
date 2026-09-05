Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'CMake\mitkInstallRules.cmake',
    'QtWebEngineProcess.exe',
    'if(EXISTS "${_qmake_path}/QtWebEngineProcess.exe")',
    'MITK_INSTALL(PROGRAMS "${_qmake_path}/QtWebEngineProcess.exe")'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK toolkit source patch must guard optional QtWebEngineProcess.exe install with EXISTS"
    }
}
