Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$CodeCMake = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\CMakeLists.txt")

if ($CodeCMake -match "WebEngineWidgets" -or $CodeCMake -match "WebEngineCore") {
    throw "Windows monolith configure must not require Qt WebEngine components"
}
