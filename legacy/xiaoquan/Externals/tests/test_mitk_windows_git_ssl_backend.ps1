Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body -notmatch '\$previousGitSslBackend\s*=\s*\$env:GIT_SSL_BACKEND') {
    throw "MITK recipe must preserve the previous GIT_SSL_BACKEND value"
}

if ($body -notmatch '\$env:GIT_SSL_BACKEND\s*=\s*"openssl"') {
    throw "MITK recipe must force Git's OpenSSL backend for superbuild GitHub downloads"
}

if ($body -notmatch 'finally\s*\{(?s).*GIT_SSL_BACKEND') {
    throw "MITK recipe must restore GIT_SSL_BACKEND after the superbuild"
}
