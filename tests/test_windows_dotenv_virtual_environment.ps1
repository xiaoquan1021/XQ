Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$DotEnvScriptPath = Join-Path $RepoRoot "scripts\xq-dotenv.ps1"
$EnterEnvScriptPath = Join-Path $RepoRoot "scripts\Enter-XQEnvironment.ps1"
$EnvExamplePath = Join-Path $RepoRoot ".env.example"
$GitIgnorePath = Join-Path $RepoRoot ".gitignore"

if (-not (Test-Path -LiteralPath $DotEnvScriptPath)) {
    throw "scripts\xq-dotenv.ps1 should provide the shared .env parser"
}
if (-not (Test-Path -LiteralPath $EnterEnvScriptPath)) {
    throw "scripts\Enter-XQEnvironment.ps1 should provide a dot-sourceable XQ environment activator"
}
if (-not (Test-Path -LiteralPath $EnvExamplePath)) {
    throw ".env.example should document the local Windows environment variables"
}

$EnvExample = Get-Content -Raw -LiteralPath $EnvExamplePath
foreach ($RequiredVariable in @(
        "XQ_EXTERNALS_ROOT",
        "XQ_BUILD_DIR",
        "XQ_EXTERNALS_PLATFORM",
        "XQ_VS_INSTALL_PATH",
        "XQ_CMAKE"
    )) {
    if ($EnvExample -notmatch "(?m)^$RequiredVariable=") {
        throw ".env.example is missing $RequiredVariable"
    }
}

$GitIgnore = Get-Content -Raw -LiteralPath $GitIgnorePath
if ($GitIgnore -notmatch "(?m)^\.env$") {
    throw ".gitignore should ignore the local .env file"
}
if ($GitIgnore -match "(?m)^\.env\.example$") {
    throw ".env.example must stay tracked"
}

. $DotEnvScriptPath

$TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("xq-dotenv-test-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $TempRoot | Out-Null
try {
    $EnvFile = Join-Path $TempRoot ".env"
    @"
# Comments and blank lines are allowed.
XQ_EXTERNALS_ROOT=..\Externals
XQ_BUILD_DIR="build\windows-msvc-release"
XQ_EXTERNALS_PLATFORM=windows-x64
XQ_VS_INSTALL_PATH='C:\software\Visual Studio\Visual Studio2022\Community'
XQ_CMAKE=C:\tools\cmake\bin\cmake.exe
"@ | Set-Content -LiteralPath $EnvFile -Encoding ASCII

    $OldExternalsRoot = [Environment]::GetEnvironmentVariable("XQ_EXTERNALS_ROOT", "Process")
    $OldBuildDir = [Environment]::GetEnvironmentVariable("XQ_BUILD_DIR", "Process")
    $OldPlatform = [Environment]::GetEnvironmentVariable("XQ_EXTERNALS_PLATFORM", "Process")
    $OldVsPath = [Environment]::GetEnvironmentVariable("XQ_VS_INSTALL_PATH", "Process")
    $OldCMake = [Environment]::GetEnvironmentVariable("XQ_CMAKE", "Process")

    [Environment]::SetEnvironmentVariable("XQ_EXTERNALS_ROOT", "already-set", "Process")
    [Environment]::SetEnvironmentVariable("XQ_BUILD_DIR", $null, "Process")
    [Environment]::SetEnvironmentVariable("XQ_EXTERNALS_PLATFORM", $null, "Process")
    [Environment]::SetEnvironmentVariable("XQ_VS_INSTALL_PATH", $null, "Process")
    [Environment]::SetEnvironmentVariable("XQ_CMAKE", $null, "Process")

    $Result = Import-XQDotEnv -RepoRoot $TempRoot

    if ($Result.EnvFile -ne $EnvFile) {
        throw "Import-XQDotEnv should load .env from the supplied repo root"
    }
    if ($env:XQ_EXTERNALS_ROOT -ne "already-set") {
        throw "Import-XQDotEnv must not override existing process variables by default"
    }
    if ($env:XQ_BUILD_DIR -ne "build\windows-msvc-release") {
        throw "Import-XQDotEnv should strip double quotes from values"
    }
    if ($env:XQ_VS_INSTALL_PATH -ne "C:\software\Visual Studio\Visual Studio2022\Community") {
        throw "Import-XQDotEnv should strip single quotes from values"
    }

    Import-XQDotEnv -RepoRoot $TempRoot -Override | Out-Null
    if ($env:XQ_EXTERNALS_ROOT -ne "..\Externals") {
        throw "Import-XQDotEnv -Override should replace existing process variables"
    }
}
finally {
    foreach ($entry in @(
            @{ Name = "XQ_EXTERNALS_ROOT"; Value = $OldExternalsRoot },
            @{ Name = "XQ_BUILD_DIR"; Value = $OldBuildDir },
            @{ Name = "XQ_EXTERNALS_PLATFORM"; Value = $OldPlatform },
            @{ Name = "XQ_VS_INSTALL_PATH"; Value = $OldVsPath },
            @{ Name = "XQ_CMAKE"; Value = $OldCMake }
        )) {
        [Environment]::SetEnvironmentVariable($entry.Name, $entry.Value, "Process")
    }
    Remove-Item -Recurse -Force -LiteralPath $TempRoot
}

$BuildScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\build-xq.ps1")
$RunScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\run-xq.ps1")
$EnvScript = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\xq-env.ps1")
$EnterScript = Get-Content -Raw -LiteralPath $EnterEnvScriptPath

foreach ($script in @(
        @{ Name = "xq-env.ps1"; Text = $EnvScript },
        @{ Name = "build-xq.ps1"; Text = $BuildScript },
        @{ Name = "run-xq.ps1"; Text = $RunScript },
        @{ Name = "Enter-XQEnvironment.ps1"; Text = $EnterScript }
    )) {
    if ($script.Text -notmatch "Import-XQDotEnv") {
        throw "$($script.Name) should import .env through Import-XQDotEnv"
    }
}

if ($BuildScript -notmatch "XQ_VS_INSTALL_PATH") {
    throw "build-xq.ps1 should allow .env to provide XQ_VS_INSTALL_PATH"
}
if ($BuildScript -notmatch "XQ_CMAKE") {
    throw "build-xq.ps1 should allow .env to provide XQ_CMAKE"
}
if ($BuildScript -match '\[string\]\$Platform\s*=\s*"windows-x64"') {
    throw "build-xq.ps1 should let .env provide XQ_EXTERNALS_PLATFORM when -Platform is omitted"
}
if ($RunScript -match '\[string\]\$Platform\s*=\s*"windows-x64"') {
    throw "run-xq.ps1 should let .env provide XQ_EXTERNALS_PLATFORM when -Platform is omitted"
}
if ($EnterScript -notmatch "Import-XQVisualStudioEnvironment") {
    throw "Enter-XQEnvironment.ps1 should activate the VS2022 x64 environment"
}
