Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$HeaderPath = Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Modules\Simulation\xq_FlowSolverRegistry.h"
$Header = Get-Content -Raw -LiteralPath $HeaderPath

if ($Header -notmatch "std::vector\s*<\s*std::unique_ptr\s*<\s*xq_FlowSolverBackend\s*>\s*>") {
    throw "Test precondition failed: xq_FlowSolverRegistry no longer owns backends through unique_ptr"
}

$RequiredDeclarations = @(
    "xq_FlowSolverRegistry\s*\(\s*const\s+xq_FlowSolverRegistry\s*&\s*\)\s*=\s*delete\s*;",
    "xq_FlowSolverRegistry\s*&\s*operator\s*=\s*\(\s*const\s+xq_FlowSolverRegistry\s*&\s*\)\s*=\s*delete\s*;",
    "xq_FlowSolverRegistry\s*\(\s*xq_FlowSolverRegistry\s*&&\s*\)\s*=\s*delete\s*;",
    "xq_FlowSolverRegistry\s*&\s*operator\s*=\s*\(\s*xq_FlowSolverRegistry\s*&&\s*\)\s*=\s*delete\s*;"
)

foreach ($Pattern in $RequiredDeclarations) {
    if ($Header -notmatch $Pattern) {
        throw "xq_FlowSolverRegistry must explicitly delete copy and move operations"
    }
}
