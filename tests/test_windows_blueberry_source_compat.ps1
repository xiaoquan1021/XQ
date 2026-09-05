Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$CodeRoot = Join-Path $RepoRoot "Code"

$direntMatches = Get-ChildItem -LiteralPath (Join-Path $CodeRoot "Source\ImagingWorkbench\Modules\ProjectManagement") -Recurse -Include *.cxx,*.cpp,*.h,*.hpp |
    Select-String -Pattern '#include\s*<dirent\.h>|opendir\s*\(|readdir\s*\(|closedir\s*\(|\bDIR\s*\*'
if ($direntMatches) {
    $locations = $direntMatches | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Windows/MSVC builds must use std::filesystem instead of POSIX dirent APIs: $($locations -join ', ')"
}

$mPiMatches = Get-ChildItem -LiteralPath $CodeRoot -Recurse -Include *.cxx,*.cpp,*.h,*.hpp |
    Select-String -Pattern "\bM_PI\b"
if ($mPiMatches) {
    $locations = $mPiMatches | ForEach-Object { "$($_.Path):$($_.LineNumber)" }
    throw "Windows/MSVC builds must not depend on non-standard M_PI: $($locations -join ', ')"
}

$testingPath = Join-Path $CodeRoot "Testing\test_seg_preprocess.cxx"
if (Test-Path -LiteralPath $testingPath) {
    $testingSource = Get-Content -Raw -LiteralPath $testingPath
    if ($testingSource -match '#pragma GCC diagnostic' -and
        $testingSource -notmatch '(?s)#if\s+defined\(__GNUC__\).*?#pragma GCC diagnostic') {
        throw "GCC diagnostic pragmas must be guarded for MSVC"
    }
}

$profileGroupHeaderPath = Join-Path $CodeRoot "Source\ImagingWorkbench\Modules\Segmentation\xq_ProfileGroup.h"
$profileGroupSourcePath = Join-Path $CodeRoot "Source\ImagingWorkbench\Modules\Segmentation\xq_ProfileGroup.cxx"
if ((Test-Path -LiteralPath $profileGroupHeaderPath) -and (Test-Path -LiteralPath $profileGroupSourcePath)) {
    $profileGroupHeader = Get-Content -Raw -LiteralPath $profileGroupHeaderPath
    $profileGroupSource = Get-Content -Raw -LiteralPath $profileGroupSourcePath

    if ($profileGroupHeader -match 'std::unique_ptr\s*<\s*xq_LumenProfile\s*>' -and
        $profileGroupHeader -match '~xq_ProfileGroup\s*\(\s*\)\s*override\s*=\s*default\s*;') {
        throw "xq_ProfileGroup owns unique_ptr<xq_LumenProfile>; keep its destructor out-of-line so MSVC sees a complete xq_LumenProfile"
    }

    if ($profileGroupHeader -match 'std::unique_ptr\s*<\s*xq_LumenProfile\s*>' -and
        $profileGroupHeader -match '~xq_ProfileGroup\s*\(\s*\)\s*override\s*;' -and
        $profileGroupSource -notmatch 'xq_ProfileGroup::~xq_ProfileGroup\s*\(\s*\)\s*=\s*default\s*;') {
        throw "xq_ProfileGroup declares an out-of-line destructor but does not define it in xq_ProfileGroup.cxx"
    }
}

$flowRegistryHeaderPath = Join-Path $CodeRoot "Source\ImagingWorkbench\Modules\Simulation\xq_FlowSolverRegistry.h"
if (Test-Path -LiteralPath $flowRegistryHeaderPath) {
    $flowRegistryHeader = Get-Content -Raw -LiteralPath $flowRegistryHeaderPath
    if ($flowRegistryHeader -match 'std::vector\s*<\s*std::unique_ptr\s*<\s*xq_FlowSolverBackend' -and
        ($flowRegistryHeader -notmatch 'xq_FlowSolverRegistry\s*\(\s*const\s+xq_FlowSolverRegistry&\s*\)\s*=\s*delete\s*;' -or
         $flowRegistryHeader -notmatch 'xq_FlowSolverRegistry&\s+operator=\s*\(\s*const\s+xq_FlowSolverRegistry&\s*\)\s*=\s*delete\s*;')) {
        throw "xq_FlowSolverRegistry owns unique_ptr backends; copy construction and copy assignment must stay deleted for MSVC"
    }
}

$projectNodesActivatorPath = Join-Path $CodeRoot "Source\ImagingWorkbench\Plugins\org.xq.data.projectnodes\src\internal\xq_ProjectDataNodesPluginActivator.cxx"
$projectNodesActivatorHeaderPath = Join-Path $CodeRoot "Source\ImagingWorkbench\Plugins\org.xq.data.projectnodes\src\internal\xq_ProjectDataNodesPluginActivator.h"
if ((Test-Path -LiteralPath $projectNodesActivatorPath) -and (Test-Path -LiteralPath $projectNodesActivatorHeaderPath)) {
    $projectNodesActivator = Get-Content -Raw -LiteralPath $projectNodesActivatorPath
    $projectNodesActivatorHeader = Get-Content -Raw -LiteralPath $projectNodesActivatorHeaderPath
    if ($projectNodesActivator -match 'xq_ProjectDataNodesPluginActivator::LoadLibrary\s*\(' -or
        $projectNodesActivatorHeader -match '\bLoadLibrary\s*\(\s*QString') {
        throw "Do not name ProjectDataNodes helper LoadLibrary; Windows headers macro-expand it to LoadLibraryW"
    }
}
