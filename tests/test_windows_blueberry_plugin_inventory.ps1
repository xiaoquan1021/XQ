Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$PluginList = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Plugins\PluginList.cmake")
$AppTargets = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\Source\Application\target_libraries.cmake")

foreach ($plugin in @(
    "org.xq.core.application",
    "org.xq.data.projectnodes",
    "org.xq.data.pythonnodes",
    "org.xq.core.datamanager",
    "org.xq.core.workspace",
    "org.xq.imaging.preprocess",
    "org.xq.imaging.centerline",
    "org.xq.imaging.lumenanalysis",
    "org.xq.imaging.volumesegmentation",
    "org.xq.imaging.anatomymodeling",
    "org.xq.imaging.volumemeshing",
    "org.xq.imaging.flowanalysis",
    "org.xq.imaging.reducedflow",
    "org.xq.imaging.multiphysics"
)) {
    if ($PluginList -notmatch [regex]::Escape("${plugin}:ON")) {
        throw "Legacy BlueBerry plugin must remain enabled: $plugin"
    }
}

$pluginXmlText = Get-ChildItem -LiteralPath (Join-Path $RepoRoot "Code\Source\ImagingWorkbench\Plugins") -Recurse -Filter plugin.xml |
    ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }
$pluginXmlText = $pluginXmlText -join "`n"

foreach ($view in @(
    "Data Manager",
    "XQ Project Manager",
    "Image Processing",
    "Path Planning",
    "2D Segmentation",
    "3D Segmentation",
    "Solid Modeling",
    "Mesh Generation",
    "Flow Simulation",
    "ROM Simulation",
    "Multi-Physics"
)) {
    if ($pluginXmlText -notmatch [regex]::Escape("name=`"$view`"")) {
        throw "Legacy BlueBerry view must remain registered: $view"
    }
}

foreach ($target in @(
    "org_mitk_gui_qt_stdmultiwidgeteditor",
    "org_mitk_gui_qt_imagenavigator",
    "org_mitk_gui_qt_viewnavigator",
    "org_mitk_gui_qt_properties",
    "org_mitk_gui_qt_measurementtoolbox",
    "org_mitk_gui_qt_pixelvalue"
)) {
    if ($AppTargets -notmatch [regex]::Escape($target)) {
        throw "XQ application must link/provision MITK Workbench target: $target"
    }
}

$provisioning = Join-Path $RepoRoot "build\windows-msvc-release\bin\XQ.provisioning"
if (Test-Path -LiteralPath $provisioning) {
    $provisioningText = Get-Content -Raw -LiteralPath $provisioning
    foreach ($plugin in @(
        "liborg_mitk_gui_qt_stdmultiwidgeteditor.dll",
        "liborg_mitk_gui_qt_imagenavigator.dll",
        "liborg_mitk_gui_qt_viewnavigator.dll",
        "liborg_mitk_gui_qt_properties.dll",
        "liborg_mitk_gui_qt_measurementtoolbox.dll",
        "liborg_mitk_gui_qt_pixelvalue.dll"
    )) {
        if ($provisioningText -notmatch [regex]::Escape($plugin)) {
            throw "XQ provisioning must include MITK Workbench plugin: $plugin"
        }
    }
}
