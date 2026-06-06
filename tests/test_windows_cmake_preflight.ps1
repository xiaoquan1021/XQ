Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$PresetPath = Join-Path $RepoRoot "CMakePresets.json"
$RuntimeScript = Join-Path $RepoRoot "scripts\xq-env.ps1"

if (-not (Test-Path -LiteralPath $PresetPath)) {
    throw "CMakePresets.json is missing"
}

$Presets = Get-Content -Raw -LiteralPath $PresetPath | ConvertFrom-Json
$WindowsPreset = $Presets.configurePresets | Where-Object { $_.name -eq "windows-msvc-release" } | Select-Object -First 1
if (-not $WindowsPreset) {
    throw "windows-msvc-release configure preset is missing"
}

if ($WindowsPreset.generator -ne "Ninja") {
    throw "windows-msvc-release must use Ninja for VS2022 command-line builds"
}

if (-not $WindowsPreset.cacheVariables.XQ_EXTERNALS_PLATFORM) {
    throw "windows-msvc-release must set XQ_EXTERNALS_PLATFORM"
}
if ($WindowsPreset.cacheVariables.XQ_BUILD_MONOLITH -ne "ON") {
    throw "windows-msvc-release must build the monolith target"
}
if ($WindowsPreset.cacheVariables.XQ_BUILD_LEGACY_BLUEBERRY -ne "OFF") {
    throw "windows-msvc-release must disable the legacy BlueBerry application by default"
}

if (-not (Test-Path -LiteralPath $RuntimeScript)) {
    throw "scripts/xq-env.ps1 is missing"
}

$RuntimeText = Get-Content -Raw -LiteralPath $RuntimeScript
if ($RuntimeText -notmatch "XQ_PLUGIN_PATH") {
    throw "Windows runtime script must configure XQ_PLUGIN_PATH"
}
if ($RuntimeText -notmatch "windows-x64") {
    throw "Windows runtime script must default to windows-x64 externals"
}
