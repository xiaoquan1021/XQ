param(
    [string]$RootDir = (Get-Location).Path,
    [string]$Platform = "windows-x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RootDir = [System.IO.Path]::GetFullPath($RootDir)
$SrcDir = Join-Path $RootDir "src"
$BuildRoot = Join-Path $RootDir (Join-Path "build" $Platform)
$InstallDir = Join-Path $RootDir (Join-Path "install" $Platform)

$XQExternalVersions = [ordered]@{
    Qt = "6.7.0"
    HDF5 = "1.14.3"
    TinyXML2 = "8.0.0"
    Python = "3.11.0"
    FreeType = "2.13.0"
    SWIG = "3.0.12"
    MMG = "5.3.9"
    GDCM = "3.0.10"
    OpenCascade = "7.6.0"
    VTK = "9.3.0"
    ITK = "5.4.0"
    MITK = "2024.06"
}

$XQExternalPaths = [ordered]@{
    Root = $RootDir
    Source = $SrcDir
    BuildRoot = $BuildRoot
    InstallRoot = $InstallDir
    Qt = Join-Path $InstallDir "qt-$($XQExternalVersions.Qt)"
    HDF5 = Join-Path $InstallDir "hdf5-$($XQExternalVersions.HDF5)"
    TinyXML2 = Join-Path $InstallDir "tinyxml2-$($XQExternalVersions.TinyXML2)"
    Python = Join-Path $InstallDir "python-$($XQExternalVersions.Python)"
    FreeType = Join-Path $InstallDir "freetype-$($XQExternalVersions.FreeType)"
    SWIG = Join-Path $InstallDir "swig-$($XQExternalVersions.SWIG)"
    MMG = Join-Path $InstallDir "mmg-$($XQExternalVersions.MMG)"
    GDCM = Join-Path $InstallDir "gdcm-$($XQExternalVersions.GDCM)"
    OpenCascade = Join-Path $InstallDir "opencascade-$($XQExternalVersions.OpenCascade)"
    VTK = Join-Path $InstallDir "vtk-$($XQExternalVersions.VTK)"
    ITK = Join-Path $InstallDir "itk-$($XQExternalVersions.ITK)"
    MITK = Join-Path $InstallDir "mitk-$($XQExternalVersions.MITK)"
}

$env:XQ_EXTERNALS_ROOT = $RootDir
$env:XQ_EXTERNALS_PLATFORM = $Platform
$env:XQ_EXTERNALS_INSTALL = $InstallDir

function Get-XQExternalVersion {
    param([Parameter(Mandatory)][string]$Name)
    return $script:XQExternalVersions[$Name]
}

function Get-XQExternalInstallPath {
    param([Parameter(Mandatory)][string]$Name)
    return $script:XQExternalPaths[$Name]
}
