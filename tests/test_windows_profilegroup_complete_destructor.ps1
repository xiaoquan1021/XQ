Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$HeaderPath = Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Modules\Segmentation\xq_ProfileGroup.h"
$SourcePath = Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Modules\Segmentation\xq_ProfileGroup.cxx"

$Header = Get-Content -Raw -LiteralPath $HeaderPath
$Source = Get-Content -Raw -LiteralPath $SourcePath

if ($Header -notmatch "std::unique_ptr\s*<\s*xq_LumenProfile\s*>") {
    throw "Test precondition failed: xq_ProfileGroup no longer owns xq_LumenProfile through unique_ptr"
}

if ($Header -match "~xq_ProfileGroup\s*\(\s*\)\s*override\s*=\s*default\s*;") {
    throw "MSVC requires xq_ProfileGroup's destructor to be defined after xq_LumenProfile is complete"
}

if ($Header -notmatch "~xq_ProfileGroup\s*\(\s*\)\s*override\s*;") {
    throw "xq_ProfileGroup should declare a non-inline destructor in the header"
}

if ($Source -notmatch '#include\s+"xq_LumenProfile\.h"') {
    throw "xq_ProfileGroup.cxx must include xq_LumenProfile.h before defaulting the destructor"
}

if ($Source -notmatch "xq_ProfileGroup::~xq_ProfileGroup\s*\(\s*\)\s*=\s*default\s*;") {
    throw "xq_ProfileGroup's destructor should be defaulted in xq_ProfileGroup.cxx"
}
