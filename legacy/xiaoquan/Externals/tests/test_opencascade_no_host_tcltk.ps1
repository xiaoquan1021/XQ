Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$occtBlock = [regex]::Match($Recipe, '(?s)"OpenCascade"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"MITK"')
if (-not $occtBlock.Success) {
    throw "OpenCascade recipe block not found"
}

$body = $occtBlock.Groups["body"].Value
foreach ($required in @(
    '"-DBUILD_MODULE_Draw=OFF"',
    '"-DUSE_TK=OFF"',
    '"-DUSE_VTK=OFF"',
    '"-DINSTALL_DIR_LAYOUT=Unix"',
    '"-U3RDPARTY_TCL_*"',
    '"-U3RDPARTY_TK_*"'
)) {
    if ($body -notmatch [regex]::Escape($required)) {
        throw "OpenCascade recipe must contain $required"
    }
}

if ($body -match '3RDPARTY_VTK_DIR') {
    throw "OpenCascade recipe should not pass 3RDPARTY_VTK_DIR when USE_VTK is off"
}
