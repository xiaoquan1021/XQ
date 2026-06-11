Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$PresetPath = Join-Path $RepoRoot "CMakePresets.json"
$RuntimeScript = Join-Path $RepoRoot "scripts\xq-env.ps1"
$BuildScript = Join-Path $RepoRoot "scripts\build-xq.ps1"
$RunScript = Join-Path $RepoRoot "scripts\run-xq.ps1"
$CodeCMakePath = Join-Path $RepoRoot "Code\CMakeLists.txt"
$XQMacrosPath = Join-Path $RepoRoot "Code\CMake\XQMacros.cmake"
$ApplicationCMakePath = Join-Path $RepoRoot "Code\Source\Application\CMakeLists.txt"

if (-not (Test-Path -LiteralPath $PresetPath)) {
    throw "CMakePresets.json is missing"
}
if (-not (Test-Path -LiteralPath $RuntimeScript)) {
    throw "scripts/xq-env.ps1 is missing"
}
if (-not (Test-Path -LiteralPath $BuildScript)) {
    throw "scripts/build-xq.ps1 is missing"
}
if (-not (Test-Path -LiteralPath $RunScript)) {
    throw "scripts/run-xq.ps1 is missing"
}

$Presets = Get-Content -Raw -LiteralPath $PresetPath | ConvertFrom-Json
$WindowsPreset = $Presets.configurePresets | Where-Object { $_.name -eq "windows-msvc-release" } | Select-Object -First 1
if (-not $WindowsPreset) {
    throw "windows-msvc-release configure preset is missing"
}
if ($WindowsPreset.generator -ne "Ninja") {
    throw "windows-msvc-release must use Ninja for VS2022 command-line builds"
}
if ($WindowsPreset.cacheVariables.XQ_EXTERNALS_PLATFORM -ne "windows-x64-blueberry") {
    throw "windows-msvc-release must target the isolated windows-x64-blueberry Externals platform"
}
if ($WindowsPreset.cacheVariables.XQ_BUILD_LEGACY_BLUEBERRY -ne "ON") {
    throw "windows-msvc-release must build the legacy BlueBerry application"
}
if ($WindowsPreset.cacheVariables.XQ_BUILD_MONOLITH -eq "ON") {
    throw "XQ1 must not build the monolith target"
}
if ($WindowsPreset.cacheVariables.XQ_BUILD_TESTING -ne "ON") {
    throw "windows-msvc-release must enable tests"
}

$RuntimeText = Get-Content -Raw -LiteralPath $RuntimeScript
$BuildText = Get-Content -Raw -LiteralPath $BuildScript
foreach ($required in @(
    "windows-x64-blueberry",
    "XQ_PLUGIN_PATH",
    "BLUEBERRY_PLUGIN_PATH",
    "QT_PLUGIN_PATH",
    "PATH"
)) {
    if ($RuntimeText -notmatch [regex]::Escape($required)) {
        throw "Windows runtime script must configure $required"
    }
}

foreach ($required in @(
    'function ConvertTo-XQCMakePath',
    'return $Path.Replace("\", "/")',
    'function Resolve-XQNinja',
    'function Resolve-XQMSVCCompiler',
    'function Resolve-XQWindowsSdkTool',
    'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe',
    'VC\Tools\MSVC',
    'Windows Kits\10\bin',
    '-DCMAKE_MAKE_PROGRAM=$(ConvertTo-XQCMakePath -Path $NinjaExe)',
    '-DCMAKE_C_COMPILER=$(ConvertTo-XQCMakePath -Path $CompilerExe)',
    '-DCMAKE_CXX_COMPILER=$(ConvertTo-XQCMakePath -Path $CompilerExe)',
    '-DCMAKE_RC_COMPILER=$(ConvertTo-XQCMakePath -Path $RcExe)',
    '-DCMAKE_MT=$(ConvertTo-XQCMakePath -Path $MtExe)',
    '[int]$Jobs = 2',
    '--parallel $Jobs'
)) {
    if ($BuildText -notmatch [regex]::Escape($required)) {
        throw "Windows build script must support low-parallelism builds via $required"
    }
}

$CodeCMake = Get-Content -Raw -LiteralPath $CodeCMakePath
if ($CodeCMake -match "WebEngineWidgets" -or $CodeCMake -match "WebEngineCore") {
    throw "Windows BlueBerry configure must not require Qt WebEngine components"
}
if ($CodeCMake -notmatch 'find_package\(Qt6 REQUIRED COMPONENTS[\s\S]*\bQuick\b') {
    throw "Windows BlueBerry configure must create Qt6::Quick before importing MITK AppUtil"
}

$ApplicationCMake = Get-Content -Raw -LiteralPath $ApplicationCMakePath
if ($ApplicationCMake -match '--allow-shlib-undefined' -and
    $ApplicationCMake -notmatch '(?s)if\s*\(\s*NOT\s+WIN32\s*\).*?target_link_options\(.*?--allow-shlib-undefined.*?endif\s*\(') {
    throw "ELF-only allow-shlib-undefined linker flag must be guarded with if(NOT WIN32)"
}

$XQMacros = Get-Content -Raw -LiteralPath $XQMacrosPath
if ($XQMacros -notmatch '(?s)if\s*\(\s*WIN32\s*\).*?set_target_properties\(\$\{XQ_PLG_TARGET\}\s+PROPERTIES\s+PREFIX\s+"lib"\s*\).*?endif\s*\(') {
    throw "Windows XQ BlueBerry plugins must use lib-prefixed DLL names so MITK provisioning resolves them"
}
