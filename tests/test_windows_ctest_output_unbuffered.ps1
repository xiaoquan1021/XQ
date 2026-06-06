Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$TestFiles = @(
    "Code\Testing\test_seg_preprocess.cxx",
    "Code\Testing\test_project_roundtrip.cxx"
)

foreach ($RelativePath in $TestFiles) {
    $Path = Join-Path $RepoRoot $RelativePath
    $Source = Get-Content -Raw -LiteralPath $Path
    if ($Source -notmatch "std::cout\.setf\s*\(\s*std::ios::unitbuf\s*\)" -or
        $Source -notmatch "std::cerr\.setf\s*\(\s*std::ios::unitbuf\s*\)") {
        throw "$RelativePath should enable unbuffered stdout/stderr so CTest timeouts show progress"
    }
}
