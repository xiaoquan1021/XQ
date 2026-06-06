Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$SourcePath = Join-Path $RepoRoot "Code\Testing\test_seg_preprocess.cxx"
$Source = Get-Content -Raw -LiteralPath $SourcePath

if ($Source -notmatch '(?s)#if\s+defined\(__GNUC__\).*?#pragma GCC diagnostic push.*?#pragma GCC diagnostic ignored "-Wdeprecated-declarations".*?#endif') {
    throw "GCC diagnostic push pragmas must be guarded so MSVC builds do not warn"
}

if ($Source -notmatch '(?s)#if\s+defined\(__GNUC__\).*?#pragma GCC diagnostic pop.*?#endif') {
    throw "GCC diagnostic pop pragma must be guarded so MSVC builds do not warn"
}
