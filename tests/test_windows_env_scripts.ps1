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
