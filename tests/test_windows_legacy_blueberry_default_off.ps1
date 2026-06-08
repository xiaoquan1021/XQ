$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$optionsPath = Join-Path $repoRoot "Code\CMake\XQOptions.cmake"
$presetPath = Join-Path $repoRoot "CMakePresets.json"

$optionsText = Get-Content -Raw -Path $optionsPath
if ($optionsText -notmatch 'option\(\s*XQ_BUILD_LEGACY_BLUEBERRY\s+"[^"]*"\s+OFF\s*\)') {
    throw "XQ_BUILD_LEGACY_BLUEBERRY must default to OFF in Code/CMake/XQOptions.cmake"
}

$preset = Get-Content -Raw -Path $presetPath | ConvertFrom-Json
$windowsPreset = $preset.configurePresets |
    Where-Object { $_.name -eq "windows-msvc-release" } |
    Select-Object -First 1

if (-not $windowsPreset) {
    throw "windows-msvc-release preset is required"
}

if ($windowsPreset.cacheVariables.XQ_BUILD_LEGACY_BLUEBERRY -ne "OFF") {
    throw "windows-msvc-release must explicitly disable legacy BlueBerry"
}
