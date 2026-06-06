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
if ($MainWindowSource -notmatch "QmitkStdMultiWidget") {
    throw "monolith MainWindow must host a QmitkStdMultiWidget render area"
}
if ($MainWindowSource -notmatch "SetDataStorage\(m_Context\.DataStorage\(\)\.GetPointer\(\)\)") {
    throw "monolith render host must use ApplicationContext DataStorage"
}
if ($MainWindowSource -notmatch "InitializeMultiWidget\(\)") {
    throw "monolith render host must initialize MITK render windows"
}
if ($MonolithMain -notmatch "QSurfaceFormat::setDefaultFormat\(QVTKOpenGLNativeWidget::defaultFormat\(\)\)") {
    throw "monolith main must initialize the QVTK OpenGL surface format before QApplication"
}
