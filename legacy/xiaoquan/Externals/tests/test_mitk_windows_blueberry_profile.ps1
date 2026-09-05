Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildAll = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "build_all.ps1")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

foreach ($required in @(
    "xq-blueberry",
    "windows-x64-blueberry"
)) {
    if ($BuildAll -notmatch [regex]::Escape($required)) {
        throw "build_all.ps1 must support $required for the legacy BlueBerry profile"
    }
}

if ($BuildAll -notmatch "Build-Dependency\.ps1.*-Profile") {
    throw "build_all.ps1 must pass the selected profile to Build-Dependency.ps1"
}

if ($Recipe -notmatch 'param\([\s\S]*\[string\]\$Profile') {
    throw "Build-Dependency.ps1 must accept a Profile parameter"
}

if ($Recipe -notmatch 'function\s+Update-XQMitkBlueBerryToolkitSource') {
    throw "MITK recipe must provide a reproducible source patch for the XQ BlueBerry build"
}

foreach ($required in @(
    "Update-XQMitkSuperbuildExternalDependencyDirs",
    "_xq_externals_same_platform_inner_mitk",
    "Update-XQMitkRuntimeSearchPaths",
    "mitkFunctionGetLibrarySearchPaths.cmake",
    "Set-XQMitkPluginListDefaults",
    "Update-XQMitkMeasurementToolboxForNoWebEngine",
    "_xq_externals_blueberry_plugin_switches",
    'MITK_BUILD_${_xq_mitk_plugin}',
    "_xq_external_dir_var ITK_DIR VTK_DIR GDCM_DIR HDF5_DIR",
    "EXTERNAL_HDF5_DIR",
    '"-DHDF5_DIR=$hdf5Dir"',
    '"-DGDCM_DIR=$gdcmDir"',
    '"-DVTK_DIR=$vtkDir"',
    '"-DITK_DIR=$itkDir"'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK BlueBerry recipe must pin same-platform runtime dependency path: $required"
    }
}

foreach ($required in @(
    "XQBlueBerry",
    "org.blueberry.core.runtime",
    "org.blueberry.core.expressions",
    "org.blueberry.ui.qt",
    "org.mitk.core.services",
    "org.mitk.gui.common",
    "org.mitk.gui.qt.common",
    "org.mitk.gui.qt.application",
    "org.mitk.gui.qt.ext",
    "org.mitk.gui.qt.extapplication",
    "org.mitk.gui.qt.datamanager",
    "org.mitk.gui.qt.mitkworkbench.intro",
    "org.mitk.gui.qt.stdmultiwidgeteditor",
    "org.mitk.gui.qt.mxnmultiwidgeteditor",
    "org.mitk.gui.qt.dicombrowser",
    "org.mitk.gui.qt.imagenavigator",
    "org.mitk.gui.qt.measurementtoolbox",
    "org.mitk.gui.qt.properties",
    "org.mitk.gui.qt.segmentation",
    "org.mitk.gui.qt.volumevisualization",
    "org.mitk.gui.qt.moviemaker",
    "org.mitk.gui.qt.pointsetinteraction",
    "org.mitk.gui.qt.remeshing",
    "org.mitk.gui.qt.viewnavigator",
    "org.mitk.gui.qt.imagecropper",
    "org.mitk.gui.qt.pixelvalue",
    '"-DMITK_BUILD_org.mitk.gui.qt.stdmultiwidgeteditor=ON"',
    '"-DMITK_BUILD_org.mitk.gui.qt.measurementtoolbox=ON"',
    '"-DMITK_BUILD_org.mitk.gui.qt.viewnavigator=ON"',
    '"QmitkImageStatisticsView.cpp"',
    '"QmitkImageStatisticsView.h"',
    "MODULE_DEPENDS MitkQtWidgetsExt MitkPlanarFigure",
    '"-DMITK_USE_BLUEBERRY=ON"',
    '"-DMITK_USE_CTK=ON"',
    '"-DBLUEBERRY_USE_QT_HELP=OFF"',
    '"-DBLUEBERRY_QT_HELP_REQUIRED=OFF"',
    '"-DMITK_WHITELIST=XQBlueBerry"'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK BlueBerry recipe must contain $required"
    }
}

if ($Recipe -match '"-DMITK_WHITELIST=XQToolkit"[\s\S]*"-DMITK_USE_BLUEBERRY=ON"') {
    throw "The existing XQToolkit profile must remain BlueBerry-free"
}
