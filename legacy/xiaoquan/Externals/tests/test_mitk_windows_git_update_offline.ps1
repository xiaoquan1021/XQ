Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'function Update-XQMitkExternalProjectGitUpdates',
    'GIT_SUBMODULES ""',
    'UPDATE_COMMAND ""',
    'GIT_CONFIG_COUNT',
    'GIT_CONFIG_KEY_0',
    'http.sslBackend',
    'GIT_CONFIG_VALUE_0',
    'openssl',
    'usr\bin'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK Windows recipe must force reproducible git behavior: $required"
    }
}

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body -notmatch 'Update-XQMitkExternalProjectGitUpdates -SourceDir \$SourceDir') {
    throw "MITK recipe must apply the git update source patch before configuring"
}

if ($body -notmatch '\$env:GIT_CONFIG_COUNT\s*=\s*"1"' -or
    $body -notmatch '\$env:GIT_CONFIG_KEY_0\s*=\s*"http\.sslBackend"' -or
    $body -notmatch '\$env:GIT_CONFIG_VALUE_0\s*=\s*"openssl"') {
    throw "MITK recipe must force Git http.sslBackend=openssl through environment config"
}

if ($body -notmatch 'Get-Command git\.exe' -or
    $body -notmatch '\$gitUsrBin = Join-Path \$gitRoot "usr\\bin"' -or
    $body -notmatch '\$env:Path = "\$gitUsrBin;\$env:Path"') {
    throw "MITK recipe must prepend Git usr\\bin so git-submodule can find sh helpers on Windows"
}

if ($body -notmatch 'Remove-Item Env:GIT_CONFIG_COUNT' -or
    $body -notmatch 'Remove-Item Env:GIT_CONFIG_KEY_0' -or
    $body -notmatch 'Remove-Item Env:GIT_CONFIG_VALUE_0' -or
    $body -notmatch 'Remove-Item Env:Path') {
    throw "MITK recipe must restore temporary Git environment overrides"
}
