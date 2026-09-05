Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$TestingCMakePath = Join-Path $RepoRoot "Code\Testing\CMakeLists.txt"

if (-not (Test-Path -LiteralPath $TestingCMakePath)) {
    throw "Code/Testing/CMakeLists.txt is missing"
}

$TestingCMake = Get-Content -Raw -LiteralPath $TestingCMakePath

if ($TestingCMake -notmatch 'ENVIRONMENT_MODIFICATION') {
    throw "Windows CTest runtime must use ENVIRONMENT_MODIFICATION for platform-native PATH updates"
}

foreach ($required in @(
    'if(WIN32)',
    'PATH=path_list_prepend:',
    'QT_PLUGIN_PATH=set:',
    'BLUEBERRY_PLUGIN_PATH=set:',
    'BLUEBERRY_PLUGIN_PATH=path_list_append:',
    '${XQ_EXTERNALS_DIR}/qt-6.7.0/bin',
    '${XQ_EXTERNALS_DIR}/hdf5-1.14.3/bin',
    '${XQ_EXTERNALS_DIR}/gdcm-3.0.10/bin',
    '${XQ_EXTERNALS_DIR}/vtk-9.3.0/bin',
    '${XQ_EXTERNALS_DIR}/itk-5.4.0/bin',
    '${XQ_EXTERNALS_DIR}/mitk-2024.06/bin',
    '${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/MITK-build/bin',
    '${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/ep/src/CTK-build/CTK-build/bin'
)) {
    if ($TestingCMake -notmatch [regex]::Escape($required)) {
        throw "Windows CTest runtime is missing required entry: $required"
    }
}

if ($TestingCMake -notmatch '(?s)else\(\).*?LD_LIBRARY_PATH=.*?endif\(\)') {
    throw "Non-Windows CTest runtime should keep LD_LIBRARY_PATH in the non-Windows branch only"
}
