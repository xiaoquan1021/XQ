Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$gdcmBlock = [regex]::Match($Recipe, '(?s)"GDCM"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"VTK"')
if (-not $gdcmBlock.Success) {
    throw "GDCM recipe block not found"
}

$body = $gdcmBlock.Groups["body"].Value
if ($body -notmatch '"-DGDCM_BUILD_DOCBOOK_MANPAGES=OFF"') {
    throw "GDCM recipe must disable DocBook manpages so xsltproc/docbook network lookups cannot break Windows builds"
}
