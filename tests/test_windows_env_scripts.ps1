Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\build-xq.ps1")
$RunScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\run-xq.ps1")

foreach ($script in @(
        @{ Name = "build-xq.ps1"; Text = $BuildScript },
        @{ Name = "run-xq.ps1"; Text = $RunScript }
    )) {
    if ($script.Text -match "powershell\.exe\s+@envArgs") {
        throw "$($script.Name) launches xq-env.ps1 in a child process; PATH changes must happen in the current process"
    }
    if ($script.Text -notmatch '\.\s+\$envScript\s+@envParams') {
        throw "$($script.Name) must dot-source xq-env.ps1 with an envParams splat"
    }
}

if ($RunScript -match '\$exeName\s*=\s*if\s*\(\$Monolith\)\s*\{\s*"XQMonolith\.exe"\s*\}\s*else\s*\{\s*"XQ\.exe"\s*\}') {
    throw "run-xq.ps1 must not hardcode -Monolith to only XQMonolith.exe"
}
if ($RunScript -notmatch '\$exeCandidates\s*=\s*if\s*\(\$Monolith\)') {
    throw "run-xq.ps1 should use executable candidates for -Monolith"
}
if ($RunScript -notmatch '@\(\s*"XQMonolith\.exe"\s*,\s*"XQ\.exe"\s*\)') {
    throw "run-xq.ps1 -Monolith should fall back from XQMonolith.exe to XQ.exe"
}
