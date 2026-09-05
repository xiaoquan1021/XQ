Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$mmgBlock = [regex]::Match($Recipe, '(?s)"MMG"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"GDCM"')
if (-not $mmgBlock.Success) {
    throw "MMG recipe block not found"
}

$body = $mmgBlock.Groups["body"].Value
if ($body -notmatch '"-DBUILD_SHARED_LIBS=ON"') {
    throw "MMG recipe must keep shared libraries enabled for the Windows dependency stack"
}

if ($body -notmatch '"-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON"') {
    throw "MMG shared build on MSVC must export symbols so import libraries exist for CLI/link targets"
}

foreach ($dll in @("mmg.dll", "mmg2d.dll", "mmgs.dll", "mmg3d.dll")) {
    if ($body -notmatch [regex]::Escape($dll)) {
        throw "MMG recipe must install $dll into the runtime bin directory"
    }
}
