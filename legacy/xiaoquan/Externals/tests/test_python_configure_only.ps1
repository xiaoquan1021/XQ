Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) ("xq-externals-python-configure-only-" + [System.Guid]::NewGuid().ToString("N"))
$PowerShellCommand = Get-Command powershell.exe -ErrorAction SilentlyContinue
if (-not $PowerShellCommand) {
    $PowerShellCommand = Get-Command pwsh -ErrorAction SilentlyContinue
}
if (-not $PowerShellCommand) {
    throw "PowerShell executable not found"
}

New-Item -ItemType Directory -Path $WorkDir | Out-Null

try {
    Copy-Item -Path (Join-Path $RepoRoot "env_variables.ps1") -Destination $WorkDir

    $sourceDir = Join-Path $WorkDir "src\Python-3.11.0"
    $pcbuildDir = Join-Path $sourceDir "PCbuild"
    New-Item -ItemType Directory -Path $pcbuildDir -Force | Out-Null

    $markerPath = Join-Path $sourceDir "build-ran.txt"
    @"
@echo off
echo built > "$markerPath"
exit /b 0
"@ | Set-Content -Path (Join-Path $pcbuildDir "build.bat")

    @"
# key|name|version|repository|official_url|target_path|order
Python|Python|3.11.0|https://example.invalid/Python.git|https://example.invalid/Python.tar.xz|src/Python-3.11.0|20
"@ | Set-Content -Path (Join-Path $WorkDir "externals.manifest")

    & $PowerShellCommand.Source -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "scripts\Build-Dependency.ps1") `
        -Dependency Python `
        -RootDir $WorkDir `
        -ConfigureOnly
    if ($LASTEXITCODE -ne 0) {
        throw "Build-Dependency.ps1 -ConfigureOnly failed"
    }

    if (Test-Path -LiteralPath $markerPath) {
        throw "Python ConfigureOnly executed PCbuild/build.bat"
    }
}
finally {
    Remove-Item -Recurse -Force -LiteralPath $WorkDir -ErrorAction SilentlyContinue
}
