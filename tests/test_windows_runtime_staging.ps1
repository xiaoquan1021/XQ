Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildBin = Join-Path $RepoRoot "build\windows-msvc-release\bin"
$Executable = Join-Path $BuildBin "XQ.exe"

if (-not (Test-Path -LiteralPath $Executable)) {
    throw "XQ.exe must be built before runtime staging can be verified"
}

$RequiredDlls = @(
    "MitkCore.dll",
    "MitkQtWidgets.dll",
    "ITKCommon-5.4.dll",
    "CppMicroServices.dll",
    "Qt6Core.dll",
    "vtkCommonCore-9.3.dll",
    "zstd.dll"
)

foreach ($dll in $RequiredDlls) {
    $candidate = Join-Path $BuildBin $dll
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Windows runtime staging must copy $dll next to XQ.exe for direct double-click startup"
    }
}

$QtPlatformPlugin = Join-Path $BuildBin "platforms\qwindows.dll"
if (-not (Test-Path -LiteralPath $QtPlatformPlugin)) {
    throw "Windows runtime staging must copy Qt platforms/qwindows.dll next to XQ.exe"
}
