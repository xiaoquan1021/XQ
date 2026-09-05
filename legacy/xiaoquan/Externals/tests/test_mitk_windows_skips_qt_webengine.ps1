Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Recipe -notmatch 'function\s+Update-XQMitkQtComponentsForToolkitBuild') {
    throw "MITK recipe must provide a reproducible source patch for toolkit Qt components"
}

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body -notmatch 'Update-XQMitkQtComponentsForToolkitBuild -SourceDir \$SourceDir') {
    throw "MITK recipe must apply the Qt component source patch before configuring"
}

if ($Recipe -notmatch 'WebEngineCore' -or $Recipe -notmatch 'WebEngineWidgets') {
    throw "MITK Qt component patch must explicitly remove WebEngineCore and WebEngineWidgets"
}

if ($Recipe -notmatch [regex]::Escape("Modules\AppUtil\CMakeLists.txt")) {
    throw "MITK Qt component patch must also patch AppUtil, which otherwise requires Qt WebEngineCore"
}

if ($Recipe -notmatch [regex]::Escape("Qt6|Widgets+Quick")) {
    throw "MITK AppUtil patch must keep Widgets+Quick while removing WebEngineCore"
}

if ($Recipe -notmatch [regex]::Escape("QWebEngineUrlScheme") -or
    $Recipe -notmatch [regex]::Escape("QT_FEATURE_webenginecore")) {
    throw "MITK AppUtil patch must guard QWebEngineUrlScheme usage when Qt WebEngineCore is unavailable"
}

if ($Recipe -match '(?m)^\s*#if\s+QT_CONFIG\(webenginecore\)\s*$') {
    throw "MITK AppUtil patch must not generate QT_CONFIG(webenginecore), which fails when the feature macro is undefined"
}

if ($Recipe -notmatch [regex]::Escape('$qtHelpSchemeRegex') -or
    $Recipe -notmatch [regex]::Escape('[regex]::Replace')) {
    throw "MITK AppUtil patch must collapse repeated WebEngine guards so reruns are idempotent"
}
