Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    'function Get-XQWindowsSdkToolPath',
    'function Update-XQMitkSuperbuildWindowsTools',
    'SuperBuild.cmake',
    'Windows Kits\10\bin',
    '-DCMAKE_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}',
    '-DCMAKE_MT:FILEPATH=${CMAKE_MT}',
    'Update-XQMitkSuperbuildWindowsTools -SourceDir $SourceDir',
    '$windowsSdkToolDir = Split-Path -Parent $rcToolPath',
    '$env:Path = "$windowsSdkToolDir;$env:Path"',
    'Get-XQWindowsSdkToolPath -ToolName "rc.exe"',
    'Get-XQWindowsSdkToolPath -ToolName "mt.exe"',
    '"-DCMAKE_RC_COMPILER=$rcExe"',
    '"-DCMAKE_MT=$mtExe"'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK Windows recipe must explicitly configure Windows SDK tools: $required"
    }
}

$mitkBlock = [regex]::Match($Recipe, '(?s)"MITK"\s*\{(?<body>.*?)\n\s*\}\s*\n\s*default')
if (-not $mitkBlock.Success) {
    throw "MITK recipe block not found"
}

$body = $mitkBlock.Groups["body"].Value
if ($body.IndexOf('$rcExe = ConvertTo-XQCMakePath', [StringComparison]::Ordinal) -gt
    $body.IndexOf('$mitkConfigureArgs = @(', [StringComparison]::Ordinal)) {
    throw "MITK recipe must resolve rc.exe before constructing configure arguments"
}

if ($body.IndexOf('$mtExe = ConvertTo-XQCMakePath', [StringComparison]::Ordinal) -gt
    $body.IndexOf('$mitkConfigureArgs = @(', [StringComparison]::Ordinal)) {
    throw "MITK recipe must resolve mt.exe before constructing configure arguments"
}
