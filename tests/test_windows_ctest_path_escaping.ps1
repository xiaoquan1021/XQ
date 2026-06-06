Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$CMakePath = Join-Path $RepoRoot "Code\Testing\CMakeLists.txt"
$Text = Get-Content -Raw -LiteralPath $CMakePath

if ($Text -notmatch "XQ_HOST_PATH_ESCAPED") {
    throw "Code/Testing CTest environment must escape the host PATH on Windows"
}

if ($Text -match 'PATH=\$\{XQ_TEST_LIBRARY_PATH_ESCAPED\}\\;\$ENV\{PATH\}') {
    throw "Code/Testing CTest environment must not append raw `$ENV{PATH} on Windows"
}

foreach ($Required in @("PATH=", "QT_PLUGIN_PATH=", "XQ_PLUGIN_PATH=")) {
    if ($Text -notmatch [regex]::Escape($Required)) {
        throw "Code/Testing CTest environment is missing $Required"
    }
}
