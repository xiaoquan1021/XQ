Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$TestingRoot = Join-Path $RepoRoot "Code\Testing"

$cppExtensions = @(".cxx", ".cpp", ".h", ".hpp")
$matches = Get-ChildItem -LiteralPath $TestingRoot -Recurse -File |
    Where-Object { $cppExtensions -contains $_.Extension } |
    Select-String -Pattern '"/home/xiaoquan|rm\s+-rf'

if ($matches) {
    $locations = $matches | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Windows tests must use generated temp data and std::filesystem cleanup: $($locations -join ', ')"
}
