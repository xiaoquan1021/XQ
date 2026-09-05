Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$vtkBlock = [regex]::Match($Recipe, '(?s)"VTK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"ITK"')
if (-not $vtkBlock.Success) {
    throw "VTK recipe block not found"
}

$body = $vtkBlock.Groups["body"].Value
if ($body -notmatch '"-DVTK_MODULE_ENABLE_VTK_GUISupportQtQuick=NO"') {
    throw "VTK recipe must disable GUISupportQtQuick; XQ needs Qt widgets rendering, not Qt Quick"
}
