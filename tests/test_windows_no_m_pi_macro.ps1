Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$CodeRoot = Join-Path $RepoRoot "Code"

$matches = Get-ChildItem -LiteralPath $CodeRoot -Recurse -Include *.cxx,*.cpp,*.h,*.hpp |
    Select-String -Pattern "\bM_PI\b"

if ($matches) {
    $locations = $matches | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Windows MSVC builds must not depend on non-standard M_PI: $($locations -join ', ')"
}
