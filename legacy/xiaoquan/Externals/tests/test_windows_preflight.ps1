Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) ("xq-externals-windows-preflight-" + [System.Guid]::NewGuid().ToString("N"))
$PowerShell = $null
$PowerShellCommand = Get-Command pwsh -ErrorAction SilentlyContinue
if (-not $PowerShellCommand) {
    $PowerShellCommand = Get-Command powershell.exe -ErrorAction SilentlyContinue
}
if ($PowerShellCommand) {
    $PowerShell = $PowerShellCommand.Source
}
if (-not $PowerShell) {
    throw "PowerShell executable not found"
}
New-Item -ItemType Directory -Path $WorkDir | Out-Null

try {
    Copy-Item -Path (Join-Path $RepoRoot "build_all.ps1") -Destination $WorkDir
    Copy-Item -Path (Join-Path $RepoRoot "env_variables.ps1") -Destination $WorkDir
    Copy-Item -Path (Join-Path $RepoRoot "externals.manifest") -Destination $WorkDir
    New-Item -ItemType Directory -Path (Join-Path $WorkDir "scripts") | Out-Null
    Copy-Item -Path (Join-Path $RepoRoot "scripts\*.ps1") -Destination (Join-Path $WorkDir "scripts")

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $Output = & $PowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $WorkDir "build_all.ps1") -Profile xq 2>&1
    $Status = $LASTEXITCODE
    $ErrorActionPreference = $PreviousErrorActionPreference

    if ($Status -eq 0) {
        throw "expected build_all.ps1 xq to fail when sources are missing"
    }

    $Text = $Output | Out-String
    if ($Text -notmatch "Missing source: src/qt-everywhere-src-6\.7\.0") {
        throw "missing Qt source preflight message was not emitted"
    }
    if ($Text -notmatch "Run: pwsh scripts/Fetch-Sources.ps1") {
        throw "missing Windows fetch hint was not emitted"
    }
    if (Test-Path (Join-Path $WorkDir "install")) {
        throw "preflight created install output before sources existed"
    }
    if (Test-Path (Join-Path $WorkDir "output")) {
        throw "preflight created build output before sources existed"
    }

    $Help = & $PowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $WorkDir "build_all.ps1") -Help 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "build_all.ps1 -Help failed"
    }
    if (($Help | Out-String) -notmatch "Build the Windows dependency stack expected by XQ") {
        throw "help output does not describe the Windows stack"
    }
}
finally {
    Remove-Item -Recurse -Force -LiteralPath $WorkDir -ErrorAction SilentlyContinue
}
