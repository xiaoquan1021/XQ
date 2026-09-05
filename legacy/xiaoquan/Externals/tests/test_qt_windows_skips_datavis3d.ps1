Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$qtBlock = [regex]::Match($Recipe, '(?s)"Qt"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"Python"')
if (-not $qtBlock.Success) {
    throw "Qt recipe block not found"
}

$body = $qtBlock.Groups["body"].Value
if ($body -notmatch '-skip\s+qtdatavis3d') {
    throw "Qt recipe must skip qtdatavis3d because the Windows source mirror lacks required mesh .obj resources"
}
