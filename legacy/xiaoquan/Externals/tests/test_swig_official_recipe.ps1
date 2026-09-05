Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Recipe -match "SWIG source does not expose CMakeLists.txt") {
    throw "SWIG recipe must not require an upstream CMakeLists.txt"
}

if ($Recipe -notmatch "Source\\CParse\\parser\.c") {
    throw "SWIG recipe must use the generated parser.c from the official release archive"
}

if ($Recipe -notmatch "swigconfig\.h") {
    throw "SWIG recipe must generate swigconfig.h for the Windows build"
}
