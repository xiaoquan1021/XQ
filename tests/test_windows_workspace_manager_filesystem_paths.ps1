Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$SourcePath = Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Modules\ProjectManagement\xq_WorkspaceManager.cxx"
$Source = Get-Content -Raw -LiteralPath $SourcePath

$legacyApis = Select-String -Path $SourcePath -Pattern '\b_mkdir\s*\(|\bmkdir\s*\(|\bstat\s*\(|\bS_IFDIR\b'
if ($legacyApis) {
    $locations = $legacyApis | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Windows path helpers should use std::filesystem instead of C/POSIX directory APIs: $($locations -join ', ')"
}

foreach ($Required in @(
    "std::filesystem::is_directory",
    "std::filesystem::is_regular_file",
    "std::filesystem::create_directory"
)) {
    if ($Source -notmatch [regex]::Escape($Required)) {
        throw "xq_WorkspaceManager path helpers should call $Required"
    }
}
