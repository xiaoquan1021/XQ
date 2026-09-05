Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Helpers = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Helpers.ps1")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Helpers -notmatch 'function\s+ConvertTo-XQCMakePath') {
    throw "Build helpers must provide ConvertTo-XQCMakePath for CMake cache/package paths"
}

$itkBlock = [regex]::Match($Recipe, '(?s)"ITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*"OpenCascade"')
if (-not $itkBlock.Success) {
    throw "ITK recipe block not found"
}

$body = $itkBlock.Groups["body"].Value
foreach ($variable in @('$qtRoot', '$qt6Dir', '$qtCmakeDir', '$vtkDir')) {
    if ($body -notmatch ([regex]::Escape($variable) + '\s*=\s*ConvertTo-XQCMakePath')) {
        throw "ITK recipe must normalize $variable with ConvertTo-XQCMakePath"
    }
}
