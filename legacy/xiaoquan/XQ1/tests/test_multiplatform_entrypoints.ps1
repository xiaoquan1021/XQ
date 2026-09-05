Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

foreach ($relativePath in @(
    "Build-XQ.cmd",
    "Run-XQ.cmd",
    "Start-XQ.cmd",
    "build-xq.sh",
    "run-xq.sh",
    "Start-XQ.sh",
    "scripts\build-xq.ps1",
    "scripts\run-xq.ps1",
    "scripts\xq-env.ps1",
    "scripts\xq-env.sh",
    "CMakePresets.json"
)) {
    $path = Join-Path $RepoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing multi-platform entrypoint: $relativePath"
    }
}

$presets = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "CMakePresets.json") | ConvertFrom-Json
$windowsPreset = $presets.configurePresets | Where-Object { $_.name -eq "windows-msvc-release" } | Select-Object -First 1
$linuxPreset = $presets.configurePresets | Where-Object { $_.name -eq "linux-release" } | Select-Object -First 1

if (-not $windowsPreset) {
    throw "windows-msvc-release preset is missing"
}
if (-not $linuxPreset) {
    throw "linux-release preset is missing"
}
if ($windowsPreset.cacheVariables.XQ_EXTERNALS_PLATFORM -ne "windows-x64-blueberry") {
    throw "Windows preset must keep using the isolated windows-x64-blueberry dependency flavor"
}
if ($linuxPreset.binaryDir -ne '${sourceDir}/build/linux-release') {
    throw "Linux preset must use build/linux-release"
}
if ($linuxPreset.cacheVariables.XQ_BUILD_LEGACY_BLUEBERRY -ne "ON") {
    throw "Linux preset must build the legacy BlueBerry Workbench"
}
if ($linuxPreset.cacheVariables.XQ_BUILD_MONOLITH -ne "OFF") {
    throw "Linux preset must not build the monolith target"
}

$buildCmd = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Build-XQ.cmd")
$runCmd = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Run-XQ.cmd")
$startCmd = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Start-XQ.cmd")
$buildSh = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "build-xq.sh")
$runSh = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "run-xq.sh")
$startSh = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Start-XQ.sh")
$envSh = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\xq-env.sh")

foreach ($required in @(
    'scripts\build-xq.ps1',
    '-ExternalsRoot "%SCRIPT_DIR%..\Externals"'
)) {
    if ($buildCmd -notmatch [regex]::Escape($required)) {
        throw "Build-XQ.cmd must dispatch to the Windows PowerShell build entrypoint with the local Externals default"
    }
}

foreach ($required in @(
    'scripts\run-xq.ps1',
    '-ExternalsRoot "%SCRIPT_DIR%..\Externals"'
)) {
    if ($runCmd -notmatch [regex]::Escape($required)) {
        throw "Run-XQ.cmd must dispatch to the Windows PowerShell run entrypoint with the local Externals default"
    }
    if ($startCmd -notmatch [regex]::Escape($required)) {
        throw "Start-XQ.cmd must dispatch to the Windows PowerShell run entrypoint with the local Externals default"
    }
}

foreach ($forbidden in @('/home/xiaoquan/XQ', '/home/xiaoquan/Externals')) {
    if ($buildSh -match [regex]::Escape($forbidden) -or $runSh -match [regex]::Escape($forbidden)) {
        throw "Linux entrypoints must not hard-code developer-local paths: $forbidden"
    }
}

foreach ($required in @(
    'source "${SCRIPT_DIR}/scripts/xq-env.sh"',
    'XQ_BUILD_DIR:-${XQ_ROOT}/build/linux-release',
    'cmake --preset "${PRESET}"',
    '-DXQ_BUILD_LEGACY_BLUEBERRY=ON',
    '-DXQ_BUILD_MONOLITH=OFF',
    'ctest --test-dir "${BUILD_DIR}" --output-on-failure',
    'timeout 10s "${exe}"'
)) {
    if ($buildSh -notmatch [regex]::Escape($required)) {
        throw "build-xq.sh is missing required Linux build behavior: $required"
    }
}

foreach ($required in @(
    'source "${SCRIPT_DIR}/scripts/xq-env.sh"',
    'XQ_BUILD_DIR:-${XQ_ROOT}/build/linux-release',
    'xq_env_apply_runtime_paths "${BUILD_DIR}"',
    'exec "${XQ_BIN}" "$@"'
)) {
    if ($runSh -notmatch [regex]::Escape($required)) {
        throw "run-xq.sh is missing required Linux runtime behavior: $required"
    }
}

if ($startSh -notmatch [regex]::Escape('exec "${SCRIPT_DIR}/run-xq.sh" "$@"')) {
    throw "Start-XQ.sh must dispatch to run-xq.sh"
}

foreach ($required in @(
    'xq_env_install_dir',
    'xq_env_mitk_config_candidates',
    'xq_env_find_mitk_build',
    'XQ_PLUGIN_PATH',
    'BLUEBERRY_PLUGIN_PATH',
    'QT_PLUGIN_PATH',
    'LD_LIBRARY_PATH'
)) {
    if ($envSh -notmatch [regex]::Escape($required)) {
        throw "scripts/xq-env.sh is missing required runtime support: $required"
    }
}
