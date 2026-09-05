Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'function Update-XQMitkCtkSourcePatch',
    'CMake\XQPatchCTK.cmake',
    'CMakeExternals\CTK.cmake',
    'qRestAPI.cmake',
    'GIT_SUBMODULES \"\"',
    'UPDATE_COMMAND \"\"',
    'PATCH_COMMAND ${CMAKE_COMMAND} -DCTK_SOURCE_DIR=<SOURCE_DIR> -P "${CMAKE_SOURCE_DIR}/CMake/XQPatchCTK.cmake"',
    'Update-XQMitkCtkSourcePatch -SourceDir $SourceDir'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK Windows recipe must disable qRestAPI git submodules inside CTK: $required"
    }
}

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body.IndexOf('Update-XQMitkCtkSourcePatch -SourceDir $SourceDir', [StringComparison]::Ordinal) -gt
    $body.IndexOf('Update-XQMitkQtComponentsForToolkitBuild -SourceDir $SourceDir', [StringComparison]::Ordinal)) {
    throw "MITK recipe should patch CTK before configuring MITK"
}
