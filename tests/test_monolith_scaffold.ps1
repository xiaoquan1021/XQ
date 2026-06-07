Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Options = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\CMake\XQOptions.cmake")
$CodeCMake = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\CMakeLists.txt")
$MonolithCMake = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\Source\Monolith\CMakeLists.txt")
$ContextHeader = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\Source\Monolith\Core\xq_ApplicationContext.h")
$MainWindowSource = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\Source\Monolith\Presentation\xq_MainWindow.cxx")
$MonolithMain = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "Code\Source\Monolith\main.cxx")

if ($Options -notmatch "XQ_BUILD_MONOLITH") {
    throw "XQ_BUILD_MONOLITH option is missing"
}
if ($CodeCMake -notmatch "Source/Monolith") {
    throw "Code/CMakeLists.txt does not add the monolith source tree"
}
if ($MonolithCMake -notmatch "add_executable\(XQMonolith") {
    throw "XQMonolith executable target is missing"
}
if ($ContextHeader -notmatch "class ApplicationContext") {
    throw "ApplicationContext public interface is missing"
}
if ($ContextHeader -notmatch "DataStorage\(\)") {
    throw "ApplicationContext must expose DataStorage()"
}
if ($CodeCMake -notmatch "if\(XQ_BUILD_LEGACY_BLUEBERRY\)[\s\S]*Source/ImagingWorkbench/Plugins/PluginList\.cmake[\s\S]*foreach\(_plugin_entry \$\{XQ_PLUGINS\}\)") {
    throw "legacy plugin directories must only be added when XQ_BUILD_LEGACY_BLUEBERRY is ON"
}
if ($MainWindowSource -match "QmitkStdMultiWidget") {
    throw "monolith MainWindow must not hard-code QmitkStdMultiWidget; inject the render host from main"
}
if ($MainWindowSource -notmatch "SetRenderHost") {
    throw "monolith MainWindow must expose a render host injection point"
}
if ($MainWindowSource -notmatch "xqRenderHostContainer") {
    throw "monolith MainWindow must keep a stable render host container"
}
if ($MonolithMain -notmatch "QmitkStdMultiWidget") {
    throw "monolith main must create the MITK render host"
}
if ($MonolithMain -notmatch "SetDataStorage\(context->DataStorage\(\)\.GetPointer\(\)\)") {
    throw "monolith render host must use ApplicationContext DataStorage"
}
if ($MonolithMain -notmatch "InitializeMultiWidget\(\)") {
    throw "monolith render host must initialize MITK render windows"
}
if ($MonolithMain -notmatch "window\.SetRenderHost\(renderHost\)") {
    throw "monolith main must inject the MITK render host into MainWindow"
}
if ($MonolithMain -notmatch "QSurfaceFormat::setDefaultFormat\(QVTKOpenGLNativeWidget::defaultFormat\(\)\)") {
    throw "monolith main must initialize the QVTK OpenGL surface format before QApplication"
}
