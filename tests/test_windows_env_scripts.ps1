Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\build-xq.ps1")
$RunScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\run-xq.ps1")
$EnvScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\xq-env.ps1")
$StartLauncherPath = Join-Path $RepoRoot "Start-XQ.cmd"

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

if ($EnvScript -notmatch [regex]::Escape("MITK\ep\src\CTK-build\CTK-build\bin")) {
    throw "xq-env.ps1 must add the MITK-built CTK bin directory so CTKWidgets.dll is available at runtime"
}

if (-not (Test-Path -LiteralPath $StartLauncherPath)) {
    throw "Start-XQ.cmd should provide a double-click launcher that uses scripts\run-xq.ps1"
}

$StartLauncher = Get-Content -Raw -LiteralPath $StartLauncherPath
if ($StartLauncher -notmatch [regex]::Escape("scripts\run-xq.ps1")) {
    throw "Start-XQ.cmd must launch through scripts\run-xq.ps1 so the runtime PATH is initialized"
}
if ($StartLauncher -match "build\\\\windows-msvc-release\\\\bin\\\\XQ\.exe") {
    throw "Start-XQ.cmd must not launch XQ.exe directly because that bypasses dependency PATH setup"
}
