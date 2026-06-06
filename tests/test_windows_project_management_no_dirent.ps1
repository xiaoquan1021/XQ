Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ProjectManagementRoot = Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Modules\ProjectManagement"

$matches = Get-ChildItem -LiteralPath $ProjectManagementRoot -Recurse -Include *.cxx,*.cpp,*.h,*.hpp |
    Select-String -Pattern '#include\s*<dirent\.h>|opendir\s*\(|readdir\s*\(|closedir\s*\(|\bDIR\s*\*'

if ($matches) {
    $locations = $matches | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Windows/MSVC builds must use std::filesystem instead of POSIX dirent APIs: $($locations -join ', ')"
}
