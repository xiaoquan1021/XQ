Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) ("xq-externals-windows-fetch-" + [System.Guid]::NewGuid().ToString("N"))
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
    $SourceRoot = Join-Path $WorkDir "sources"
    $ExternalRoot = Join-Path $WorkDir "externals"
    New-Item -ItemType Directory -Path $SourceRoot | Out-Null
    New-Item -ItemType Directory -Path $ExternalRoot | Out-Null

    foreach ($Name in @("Qt", "MITK")) {
        $repo = Join-Path $SourceRoot $Name
        New-Item -ItemType Directory -Path $repo | Out-Null
        git -C $repo init -q
        "source for $Name" | Set-Content -Path (Join-Path $repo "README.md")
        git -C $repo add README.md
        git -C $repo -c user.name=test -c user.email=test@example.invalid commit -q -m init
    }

    $manifest = Join-Path $WorkDir "externals.manifest"
    @"
# key|name|version|repository|official_url|target_path|order
Qt|Qt|6.7.0|file://$($SourceRoot -replace "\\", "/")/Qt|https://example.invalid/qt.tar.xz|src/qt-everywhere-src-6.7.0|10
MITK|MITK|2024.06|file://$($SourceRoot -replace "\\", "/")/MITK|https://example.invalid/mitk.tar.gz|src/MITK-2024.06|20
"@ | Set-Content -Path $manifest

    $dryRun = & $PowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "scripts\Fetch-Sources.ps1") `
        -RootDir $ExternalRoot `
        -ManifestPath $manifest `
        -Target Qt `
        -DryRun
    if (($dryRun | Out-String) -notmatch "clone Qt -> src/qt-everywhere-src-6.7.0") {
        throw "dry-run did not show the Qt clone operation"
    }
    if (Test-Path -LiteralPath (Join-Path $ExternalRoot "src\qt-everywhere-src-6.7.0")) {
        throw "dry-run created source output"
    }

    & $PowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "scripts\Fetch-Sources.ps1") `
        -RootDir $ExternalRoot `
        -ManifestPath $manifest `
        -Target all
    if ($LASTEXITCODE -ne 0) {
        throw "Fetch-Sources.ps1 all failed"
    }

    if (-not (Test-Path -LiteralPath (Join-Path $ExternalRoot "src\qt-everywhere-src-6.7.0\README.md"))) {
        throw "Qt source was not cloned from manifest"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $ExternalRoot "src\MITK-2024.06\README.md"))) {
        throw "MITK source was not cloned from manifest"
    }
}
finally {
    Remove-Item -Recurse -Force -LiteralPath $WorkDir -ErrorAction SilentlyContinue
}
