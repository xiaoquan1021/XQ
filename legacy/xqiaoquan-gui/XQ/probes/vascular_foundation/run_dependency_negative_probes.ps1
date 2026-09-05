[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepoRoot,
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string]$PrefixPath,
    [Parameter(Mandatory)][string]$QtDir,
    [Parameter(Mandatory)][string]$VtkDir,
    [Parameter(Mandatory)][string]$ItkDir,
    [Parameter(Mandatory)][string]$TinyXml2Dir,
    [Parameter(Mandatory)][string]$PythonRoot,
    [Parameter(Mandatory)][string]$TestDataRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$CMake = (Resolve-Path -LiteralPath $CMake).Path
$PythonRoot = (Resolve-Path -LiteralPath $PythonRoot).Path
$negativeRoot = Join-Path $RepoRoot ".trellis\workspace\ocean\dependency-remediation-negative"

function Invoke-ExpectedConfigureFailure {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$ExpectedMessage,
        [Parameter(Mandatory)][string[]]$ExtraArgs
    )

    $buildDir = Join-Path $negativeRoot $Name
    $args = @(
        "--fresh",
        "-S", (Join-Path $RepoRoot "XQ"),
        "-B", $buildDir,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_PREFIX_PATH=$PrefixPath",
        "-DQt6_DIR=$QtDir",
        "-DVTK_DIR=$VtkDir",
        "-DITK_DIR=$ItkDir",
        "-Dtinyxml2_DIR=$TinyXml2Dir",
        "-DPython3_ROOT_DIR=$PythonRoot",
        "-DPython3_EXECUTABLE=$(Join-Path $PythonRoot 'python.exe')",
        "-DPython3_INCLUDE_DIR=$(Join-Path $PythonRoot 'include')",
        "-DPython3_LIBRARY=$(Join-Path $PythonRoot 'libs\python311.lib')",
        "-DXQ_TEST_DATA_ROOT=$TestDataRoot",
        "-DXQ_DICOM_TEST_DATA_ROOT="
    ) + $ExtraArgs

    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = (& $CMake @args 2>&1 | Out-String)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($exitCode -eq 0) {
        throw "Negative configure '$Name' unexpectedly succeeded"
    }
    if ($output -notmatch [regex]::Escape($ExpectedMessage)) {
        throw "Negative configure '$Name' missed expected diagnostic '$ExpectedMessage':`n$output"
    }
    Write-Host "[XQ dependency baseline] negative probe passed: $Name"
}

function Invoke-StandaloneExpectedFailure {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$ExpectedMessage,
        [Parameter(Mandatory)][string]$CMakeLists,
        [Parameter(Mandatory)][string[]]$ConfigureArgs
    )

    $sourceDir = Join-Path $negativeRoot "source-$Name"
    $buildDir = Join-Path $negativeRoot "build-$Name"
    New-Item -ItemType Directory -Path $sourceDir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir "CMakeLists.txt") `
        -Value $CMakeLists -Encoding UTF8

    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = (& $CMake --fresh -S $sourceDir -B $buildDir -G Ninja @ConfigureArgs 2>&1 | Out-String)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($exitCode -eq 0) {
        throw "Standalone negative configure '$Name' unexpectedly succeeded"
    }
    if ($output -notmatch [regex]::Escape($ExpectedMessage)) {
        throw "Standalone negative configure '$Name' missed expected diagnostic '$ExpectedMessage':`n$output"
    }
    Write-Host "[XQ dependency baseline] negative probe passed: $Name"
}

$missingRoot = Join-Path $negativeRoot "missing-package"
Invoke-ExpectedConfigureFailure -Name "bad-qt" `
    -ExpectedMessage "Qt6_DIR must name a package directory containing Qt6Config.cmake" `
    -ExtraArgs @("-DQt6_DIR=$(Join-Path $missingRoot 'Qt6')")
Invoke-ExpectedConfigureFailure -Name "bad-vtk" `
    -ExpectedMessage "VTK_DIR must name a package directory containing vtk-config.cmake" `
    -ExtraArgs @("-DVTK_DIR=$(Join-Path $missingRoot 'VTK')")
Invoke-ExpectedConfigureFailure -Name "bad-itk" `
    -ExpectedMessage "ITK_DIR must name a package directory containing ITKConfig.cmake" `
    -ExtraArgs @("-DITK_DIR=$(Join-Path $missingRoot 'ITK')")
Invoke-ExpectedConfigureFailure -Name "empty-qt" `
    -ExpectedMessage "Qt6_DIR was supplied but is empty" `
    -ExtraArgs @("-DQt6_DIR=")
Invoke-ExpectedConfigureFailure -Name "tetgen-no-ack" `
    -ExpectedMessage "XQ_ENABLE_TETGEN=ON is research-only" `
    -ExtraArgs @("-DXQ_ENABLE_TETGEN=ON")

Invoke-StandaloneExpectedFailure -Name "wrong-qt-version" `
    -ExpectedMessage 'requested version "6.7.1"' `
    -CMakeLists @'
cmake_minimum_required(VERSION 3.20)
project(xq_wrong_qt_version NONE)
find_package(Qt6 6.7.1 EXACT CONFIG REQUIRED COMPONENTS Core Widgets NO_DEFAULT_PATH)
'@ `
    -ConfigureArgs @("-DQt6_DIR=$QtDir")

Invoke-StandaloneExpectedFailure -Name "missing-itk-component" `
    -ExpectedMessage "ITKDefinitelyMissing" `
    -CMakeLists @'
cmake_minimum_required(VERSION 3.20)
project(xq_missing_itk_component CXX)
find_package(ITK 5.4.0 EXACT CONFIG REQUIRED COMPONENTS ITKDefinitelyMissing NO_DEFAULT_PATH)
'@ `
    -ConfigureArgs @(
        "-DITK_DIR=$ItkDir",
        "-DVTK_DIR=$VtkDir",
        "-DQt6_DIR=$QtDir",
        "-DPython3_ROOT_DIR=$PythonRoot",
        "-DPython3_EXECUTABLE=$(Join-Path $PythonRoot 'python.exe')",
        "-DPython3_INCLUDE_DIR=$(Join-Path $PythonRoot 'include')",
        "-DPython3_LIBRARY=$(Join-Path $PythonRoot 'libs\python311.lib')",
        "-DCMAKE_PREFIX_PATH=$PrefixPath"
    )
