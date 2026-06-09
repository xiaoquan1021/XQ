Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$handlerRoot = Join-Path $repoRoot "Code\Source\Monolith\Infrastructure"
$handlers = Get-ChildItem -Path $handlerRoot -Filter "xq_*WorkflowActionHandler.cxx"

if (-not $handlers) {
    throw "No monolith Infrastructure workflow handlers were found."
}

$blockedPatterns = @(
    "operation accepted",
    "domain workflow accepted"
)

$violations = @()
foreach ($handler in $handlers) {
    $matches = Select-String -Path $handler.FullName -Pattern $blockedPatterns -SimpleMatch
    foreach ($match in $matches) {
        $violations += "{0}:{1}: {2}" -f $handler.Name, $match.LineNumber, $match.Line.Trim()
    }
}

if ($violations.Count -gt 0) {
    throw "Infrastructure workflow handlers must not report placeholder success:`n$($violations -join "`n")"
}
