param(
    [Parameter(Mandatory)][string]$Dependency,
    [string]$Profile = "xq",
    [string]$RootDir = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..")).Path,
    [string]$Platform = "windows-x64",
    [string]$BuildType = "Release",
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 1),
    [switch]$ConfigureOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $RootDir "env_variables.ps1") -RootDir $RootDir -Platform $Platform
. (Join-Path $ScriptDir "Build-Helpers.ps1")

$ManifestPath = Join-Path $RootDir "externals.manifest"
$Entry = Get-XQManifestEntries -ManifestPath $ManifestPath |
    Where-Object { $_.Key.Equals($Dependency, [StringComparison]::OrdinalIgnoreCase) -or $_.Name.Equals($Dependency, [StringComparison]::OrdinalIgnoreCase) } |
    Select-Object -First 1

if (-not $Entry) {
    throw "[Externals][ERROR] Dependency not found in manifest: $Dependency"
}

$Dependency = $Entry.Key
$SourceDir = Join-Path $RootDir $Entry.TargetPath
$BuildDir = Join-Path $XQExternalPaths.BuildRoot $Dependency
$InstallDir = Get-XQExternalInstallPath -Name $Dependency
$CMake = if ($env:XQ_CMAKE_EXE) { $env:XQ_CMAKE_EXE } else { "cmake" }

function Invoke-XQConfigureOnlyGuard {
    if ($ConfigureOnly) {
        Write-Host "[Externals] Configure-only requested; skipping build/install for $Dependency."
        return $true
    }
    return $false
}

function Update-XQMitkExternalProjectGitUpdates {
    param([Parameter(Mandatory)][string]$SourceDir)

    $cmakeExternalsDir = Join-Path $SourceDir "CMakeExternals"
    if (-not (Test-Path -LiteralPath $cmakeExternalsDir -PathType Container)) {
        throw "[Externals][ERROR] MITK CMakeExternals directory not found: $cmakeExternalsDir"
    }

    $externalProjectPattern = '(?s)ExternalProject_Add\(\$\{proj\}(?<body>.*?)(?<close>\r?\n\s*\))'
    $gitTagLinePattern = '(?m)^(?<indent>\s*)GIT_TAG\b[^\r\n]*(?<eol>\r?\n)'
    $gitRepositoryLinePattern = '(?m)^(?<indent>\s*)GIT_REPOSITORY\b[^\r\n]*(?<eol>\r?\n)'

    Get-ChildItem -LiteralPath $cmakeExternalsDir -Filter "*.cmake" | ForEach-Object {
        $content = Get-Content -Raw -LiteralPath $_.FullName
        $updated = [regex]::Replace($content, $externalProjectPattern, {
            param($match)

            $body = $match.Groups["body"].Value
            if ($body -notmatch '(?m)^\s*GIT_REPOSITORY\b') {
                return $match.Value
            }

            $updatedBlock = $match.Value
            $gitTagRegex = [regex]$gitTagLinePattern
            $gitRepositoryRegex = [regex]$gitRepositoryLinePattern
            $anchorRegex = if ($gitTagRegex.IsMatch($updatedBlock)) { $gitTagRegex } else { $gitRepositoryRegex }

            if ($body -notmatch '(?m)^\s*GIT_SUBMODULES\s+""') {
                $updatedBlock = $anchorRegex.Replace($updatedBlock, {
                    param($lineMatch)
                    return $lineMatch.Value + $lineMatch.Groups["indent"].Value + 'GIT_SUBMODULES ""' + $lineMatch.Groups["eol"].Value
                }, 1)
            }

            if ($body -notmatch '(?m)^\s*UPDATE_COMMAND\s+""') {
                $gitSubmodulesLinePattern = '(?m)^(?<indent>\s*)GIT_SUBMODULES\s+""(?<eol>\r?\n)'
                $gitSubmodulesRegex = [regex]$gitSubmodulesLinePattern
                $updatedBlock = $gitSubmodulesRegex.Replace($updatedBlock, {
                    param($lineMatch)
                    return $lineMatch.Value + $lineMatch.Groups["indent"].Value + 'UPDATE_COMMAND ""' + $lineMatch.Groups["eol"].Value
                }, 1)
            }

            return $updatedBlock
        })

        if ($updated -ne $content) {
            Write-Host "[Externals] Disabling MITK ExternalProject git update step in $($_.Name)."
            Set-Content -LiteralPath $_.FullName -Value $updated
        }
    }
}

function Get-XQWindowsSdkToolPath {
    param([Parameter(Mandatory)][string]$ToolName)

    $candidateDirs = @()
    if ($env:WindowsSdkVerBinPath) {
        $candidateDirs += (Join-Path $env:WindowsSdkVerBinPath "x64")
        $candidateDirs += $env:WindowsSdkVerBinPath
    }
    if ($env:WindowsSdkBinPath) {
        $candidateDirs += (Join-Path $env:WindowsSdkBinPath "x64")
        $candidateDirs += $env:WindowsSdkBinPath
    }

    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (Test-Path -LiteralPath $kitsRoot -PathType Container) {
        $candidateDirs += Get-ChildItem -LiteralPath $kitsRoot -Directory |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "x64" }
    }

    foreach ($dir in $candidateDirs) {
        if (-not $dir) {
            continue
        }

        $candidate = Join-Path $dir $ToolName
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $command = Get-Command $ToolName -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) {
        return $command.Source
    }

    throw "[Externals][ERROR] Windows SDK tool not found: $ToolName"
}

function Update-XQMitkSuperbuildWindowsTools {
    param([Parameter(Mandatory)][string]$SourceDir)

    $superBuild = Join-Path $SourceDir "SuperBuild.cmake"
    if (-not (Test-Path -LiteralPath $superBuild)) {
        throw "[Externals][ERROR] MITK SuperBuild.cmake not found: $superBuild"
    }

    $content = Get-Content -Raw -LiteralPath $superBuild
    if ($content.Contains('-DCMAKE_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}') -and
        $content.Contains('-DCMAKE_MT:FILEPATH=${CMAKE_MT}')) {
        return
    }

    $anchor = '  -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}'
    $replacement = @'
  -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
  -DCMAKE_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}
  -DCMAKE_MT:FILEPATH=${CMAKE_MT}
'@

    if (-not $content.Contains($anchor)) {
        throw "[Externals][ERROR] MITK SuperBuild ep_common_args compiler anchor not found."
    }

    Write-Host "[Externals] Patching MITK SuperBuild to forward Windows SDK tools to ExternalProject builds."
    Set-Content -LiteralPath $superBuild -Value $content.Replace($anchor, $replacement)
}

function Update-XQMitkSuperbuildExternalDependencyDirs {
    param([Parameter(Mandatory)][string]$SourceDir)

    $superBuild = Join-Path $SourceDir "SuperBuild.cmake"
    if (-not (Test-Path -LiteralPath $superBuild)) {
        throw "[Externals][ERROR] MITK SuperBuild.cmake not found: $superBuild"
    }

    $content = Get-Content -Raw -LiteralPath $superBuild
    if ($content.Contains("_xq_externals_same_platform_inner_mitk") -and
        -not $content.Contains("_xq_externals_blueberry_plugin_switches")) {
        $pluginAnchor = @'
endif()

set(proj MITK-Configure)
'@
        $pluginPassThrough = @'
endif()

# _xq_externals_blueberry_plugin_switches
foreach(_xq_mitk_plugin IN ITEMS
  org.mitk.gui.qt.stdmultiwidgeteditor
  org.mitk.gui.qt.mxnmultiwidgeteditor
  org.mitk.gui.qt.imagenavigator
  org.mitk.gui.qt.measurementtoolbox
  org.mitk.gui.qt.viewnavigator
  org.mitk.gui.qt.pixelvalue
)
  set(_xq_mitk_build_var "MITK_BUILD_${_xq_mitk_plugin}")
  if(DEFINED ${_xq_mitk_build_var})
    list(APPEND mitk_optional_cache_args "-D${_xq_mitk_build_var}:BOOL=${${_xq_mitk_build_var}}")
  endif()
endforeach()

set(proj MITK-Configure)
'@
        if (-not $content.Contains($pluginAnchor)) {
            throw "[Externals][ERROR] MITK SuperBuild plugin-pass-through anchor not found."
        }

        Write-Host "[Externals] Patching MITK SuperBuild to pass BlueBerry plugin switches to inner MITK configure."
        Set-Content -LiteralPath $superBuild -Value $content.Replace($pluginAnchor, $pluginPassThrough)
        return
    }

    if ($content.Contains("_xq_externals_same_platform_inner_mitk")) {
        return
    }

    $anchor = @'
if(MITK_DOXYGEN_BUILD_ALWAYS)
  list(APPEND mitk_optional_cache_args
    -DMITK_DOXYGEN_BUILD_ALWAYS:BOOL=${MITK_DOXYGEN_BUILD_ALWAYS}
  )
endif()

set(proj MITK-Configure)
'@

    $replacement = @'
if(MITK_DOXYGEN_BUILD_ALWAYS)
  list(APPEND mitk_optional_cache_args
    -DMITK_DOXYGEN_BUILD_ALWAYS:BOOL=${MITK_DOXYGEN_BUILD_ALWAYS}
  )
endif()

# _xq_externals_same_platform_inner_mitk
if(DEFINED EXTERNAL_GDCM_DIR AND NOT "${EXTERNAL_GDCM_DIR}" STREQUAL "")
  list(APPEND mitk_optional_cache_args
    "-DEXTERNAL_GDCM_DIR:PATH=${EXTERNAL_GDCM_DIR}"
    "-DGDCM_DIR:PATH=${EXTERNAL_GDCM_DIR}/lib/gdcm-3.0"
  )
endif()
if(DEFINED EXTERNAL_HDF5_DIR AND NOT "${EXTERNAL_HDF5_DIR}" STREQUAL "")
  list(APPEND mitk_optional_cache_args
    "-DEXTERNAL_HDF5_DIR:PATH=${EXTERNAL_HDF5_DIR}"
    "-DHDF5_DIR:PATH=${EXTERNAL_HDF5_DIR}/cmake"
  )
endif()
if(DEFINED EXTERNAL_ITK_DIR AND NOT "${EXTERNAL_ITK_DIR}" STREQUAL "")
  list(APPEND mitk_optional_cache_args
    "-DEXTERNAL_ITK_DIR:PATH=${EXTERNAL_ITK_DIR}"
    "-DITK_DIR:PATH=${EXTERNAL_ITK_DIR}/lib/cmake/ITK-5.4"
  )
endif()
if(DEFINED EXTERNAL_VTK_DIR AND NOT "${EXTERNAL_VTK_DIR}" STREQUAL "")
  list(APPEND mitk_optional_cache_args
    "-DEXTERNAL_VTK_DIR:PATH=${EXTERNAL_VTK_DIR}"
    "-DVTK_DIR:PATH=${EXTERNAL_VTK_DIR}/lib/cmake/vtk-9.3"
  )
endif()

# _xq_externals_blueberry_plugin_switches
foreach(_xq_mitk_plugin IN ITEMS
  org.mitk.gui.qt.stdmultiwidgeteditor
  org.mitk.gui.qt.mxnmultiwidgeteditor
  org.mitk.gui.qt.imagenavigator
  org.mitk.gui.qt.measurementtoolbox
  org.mitk.gui.qt.viewnavigator
  org.mitk.gui.qt.pixelvalue
)
  set(_xq_mitk_build_var "MITK_BUILD_${_xq_mitk_plugin}")
  if(DEFINED ${_xq_mitk_build_var})
    list(APPEND mitk_optional_cache_args "-D${_xq_mitk_build_var}:BOOL=${${_xq_mitk_build_var}}")
  endif()
endforeach()

set(proj MITK-Configure)
'@

    if (-not $content.Contains($anchor)) {
        throw "[Externals][ERROR] MITK SuperBuild optional-cache anchor not found."
    }

    Write-Host "[Externals] Patching MITK SuperBuild to pass same-platform external dependency dirs to inner MITK configure."
    Set-Content -LiteralPath $superBuild -Value $content.Replace($anchor, $replacement)
}

function Update-XQMitkCtkSourcePatch {
    param([Parameter(Mandatory)][string]$SourceDir)

    $patchScript = Join-Path $SourceDir "CMake\XQPatchCTK.cmake"
    $patchScriptContent = @'
if(NOT DEFINED CTK_SOURCE_DIR)
  message(FATAL_ERROR "CTK_SOURCE_DIR is required")
endif()

set(qrestapi_file "${CTK_SOURCE_DIR}/CMakeExternals/qRestAPI.cmake")
if(EXISTS "${qrestapi_file}")
  file(READ "${qrestapi_file}" qrestapi_content)
  if(NOT qrestapi_content MATCHES "GIT_SUBMODULES")
    string(REPLACE "    \${location_args}
    INSTALL_COMMAND \"\"" "    \${location_args}
    GIT_SUBMODULES \"\"
    UPDATE_COMMAND \"\"
    INSTALL_COMMAND \"\"" qrestapi_content "${qrestapi_content}")
    file(WRITE "${qrestapi_file}" "${qrestapi_content}")
  endif()
endif()
'@

    if ((-not (Test-Path -LiteralPath $patchScript)) -or
        ((Get-Content -Raw -LiteralPath $patchScript) -ne $patchScriptContent)) {
        Write-Host "[Externals] Writing MITK CTK source patch script."
        Set-Content -LiteralPath $patchScript -Value $patchScriptContent
    }

    $ctkCMake = Join-Path $SourceDir "CMakeExternals\CTK.cmake"
    if (-not (Test-Path -LiteralPath $ctkCMake)) {
        throw "[Externals][ERROR] MITK CTK external recipe not found: $ctkCMake"
    }

    $ctkCMakeContent = Get-Content -Raw -LiteralPath $ctkCMake
    if ($ctkCMakeContent.Contains("XQPatchCTK.cmake")) {
        return
    }

    $installCommand = '      INSTALL_COMMAND ""'
    $patchCommand = @'
      PATCH_COMMAND ${CMAKE_COMMAND} -DCTK_SOURCE_DIR=<SOURCE_DIR> -P "${CMAKE_SOURCE_DIR}/CMake/XQPatchCTK.cmake"
      INSTALL_COMMAND ""
'@
    if (-not $ctkCMakeContent.Contains($installCommand)) {
        throw "[Externals][ERROR] MITK CTK external install-command anchor not found."
    }

    Write-Host "[Externals] Patching MITK CTK external to disable qRestAPI git submodules."
    Set-Content -LiteralPath $ctkCMake -Value $ctkCMakeContent.Replace($installCommand, $patchCommand)
}

function Update-XQMitkQtComponentsForToolkitBuild {
    param([Parameter(Mandatory)][string]$SourceDir)

    $cmakeLists = Join-Path $SourceDir "CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $cmakeLists)) {
        throw "[Externals][ERROR] MITK CMakeLists.txt not found: $cmakeLists"
    }

    $content = Get-Content -Raw -LiteralPath $cmakeLists
    $updated = $content `
        -replace "(?m)^\s+WebEngineCore\s*\r?\n", "" `
        -replace "(?m)^\s+WebEngineWidgets\s*\r?\n", ""

    if ($updated -ne $content) {
        Write-Host "[Externals] Patching MITK Qt component list for toolkit build without Qt WebEngine."
        Set-Content -LiteralPath $cmakeLists -Value $updated
    }

    $appUtilCMake = Join-Path $SourceDir "Modules\AppUtil\CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $appUtilCMake)) {
        throw "[Externals][ERROR] MITK AppUtil CMakeLists.txt not found: $appUtilCMake"
    }

    $appUtilCMakeContent = Get-Content -Raw -LiteralPath $appUtilCMake
    $appUtilCMakeUpdated = $appUtilCMakeContent -replace "Qt6\|Widgets\+WebEngineCore\+Quick", "Qt6|Widgets+Quick"
    if ($appUtilCMakeUpdated -ne $appUtilCMakeContent) {
        Write-Host "[Externals] Patching MITK AppUtil Qt dependencies for toolkit build without Qt WebEngineCore."
        Set-Content -LiteralPath $appUtilCMake -Value $appUtilCMakeUpdated
    }

    $baseApplication = Join-Path $SourceDir "Modules\AppUtil\src\mitkBaseApplication.cpp"
    if (-not (Test-Path -LiteralPath $baseApplication)) {
        throw "[Externals][ERROR] MITK BaseApplication source not found: $baseApplication"
    }

    $baseApplicationContent = Get-Content -Raw -LiteralPath $baseApplication
    $baseApplicationUpdated = $baseApplicationContent
    $webEngineFeatureGuard = "#if defined(QT_FEATURE_webenginecore) && (QT_FEATURE_webenginecore == 1)"
    $baseApplicationUpdated = $baseApplicationUpdated.Replace("#if QT_CONFIG(webenginecore)", $webEngineFeatureGuard)
    $qtHelpSchemeRegex = '(?s)(?:#if defined\(QT_FEATURE_webenginecore\) && \(QT_FEATURE_webenginecore == 1\)\s*)+(\s*QWebEngineUrlScheme qtHelpScheme\("qthelp"\);\s*qtHelpScheme\.setFlags\(QWebEngineUrlScheme::LocalScheme \| QWebEngineUrlScheme::LocalAccessAllowed\);\s*QWebEngineUrlScheme::registerScheme\(qtHelpScheme\);\s*)(?:#endif\s*)+'
    $baseApplicationUpdated = [regex]::Replace(
        $baseApplicationUpdated,
        $qtHelpSchemeRegex,
        "$webEngineFeatureGuard`r`n`$1#endif`r`n")

    if ($baseApplicationUpdated.Contains("#include <QWebEngineUrlScheme>") -and
        -not $baseApplicationUpdated.Contains("QT_FEATURE_webenginecore")) {
        Write-Host "[Externals] Guarding MITK AppUtil QWebEngineUrlScheme include."
        $baseApplicationUpdated = $baseApplicationUpdated.Replace(
            "#include <QWebEngineUrlScheme>",
            @'
#include <QtGlobal>
#if defined(QT_FEATURE_webenginecore) && (QT_FEATURE_webenginecore == 1)
#include <QWebEngineUrlScheme>
#endif
'@)
    }

    $legacyQtHelpSchemeRegistration = @'
    QWebEngineUrlScheme qtHelpScheme("qthelp");
    qtHelpScheme.setFlags(QWebEngineUrlScheme::LocalScheme | QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(qtHelpScheme);
'@
    $guardedQtHelpSchemeRegistration = @'
#if defined(QT_FEATURE_webenginecore) && (QT_FEATURE_webenginecore == 1)
    QWebEngineUrlScheme qtHelpScheme("qthelp");
    qtHelpScheme.setFlags(QWebEngineUrlScheme::LocalScheme | QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(qtHelpScheme);
#endif
'@
    if ($baseApplicationUpdated.Contains($legacyQtHelpSchemeRegistration) -and
        -not $baseApplicationUpdated.Contains($guardedQtHelpSchemeRegistration)) {
        Write-Host "[Externals] Guarding MITK AppUtil qthelp WebEngine scheme registration."
        $baseApplicationUpdated = $baseApplicationUpdated.Replace($legacyQtHelpSchemeRegistration, $guardedQtHelpSchemeRegistration)
    }

    if ($baseApplicationUpdated -ne $baseApplicationContent) {
        Set-Content -LiteralPath $baseApplication -Value $baseApplicationUpdated
    }
}

function Update-XQMitkToolkitSource {
    param([Parameter(Mandatory)][string]$SourceDir)

    $whitelistDir = Join-Path $SourceDir "CMake\Whitelists"
    if (-not (Test-Path -LiteralPath $whitelistDir -PathType Container)) {
        throw "[Externals][ERROR] MITK whitelist directory not found: $whitelistDir"
    }

    # Excluded from this toolkit build: AppUtil, Chart, ImageStatisticsUI,
    # SegmentationUI, and other modules that require BlueBerry, CTK plugins,
    # Qt WebEngineCore, or Qt WebEngineWidgets.
    $whitelistPath = Join-Path $whitelistDir "XQToolkit.cmake"
    $whitelist = @'
set(enabled_modules
  Log
  Core
  DataTypesExt
  Annotation
  LegacyGL
  AlgorithmsExt
  MapperExt
  DICOM
  DICOMQI
  SceneSerializationBase
  PlanarFigure
  ImageExtraction
  SceneSerialization
  GraphAlgorithms
  Multilabel
  ImageStatistics
  ContourModel
  SurfaceInterpolation
  BoundingShape
  Segmentation
  QtWidgets
  LegacyIO
  IOExt
  ROI
)

set(enabled_plugins
)
'@
    if ((-not (Test-Path -LiteralPath $whitelistPath)) -or
        ((Get-Content -Raw -LiteralPath $whitelistPath) -ne $whitelist)) {
        Write-Host "[Externals] Writing MITK XQToolkit whitelist."
        Set-Content -LiteralPath $whitelistPath -Value $whitelist
    }

    $segmentationCMake = Join-Path $SourceDir "Modules\Segmentation\CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $segmentationCMake)) {
        throw "[Externals][ERROR] MITK Segmentation CMakeLists.txt not found: $segmentationCMake"
    }

    $httplibDependency = "PUBLIC ITK|QuadEdgeMesh+RegionGrowing httplib"
    $segmentationCMakeContent = Get-Content -Raw -LiteralPath $segmentationCMake
    $segmentationCMakeUpdated = $segmentationCMakeContent -replace ([regex]::Escape($httplibDependency) + "\s*"), "PUBLIC ITK|QuadEdgeMesh+RegionGrowing`r`n"
    $segmentationCMakeUpdated = $segmentationCMakeUpdated -replace "PUBLIC ITK\|QuadEdgeMesh\+RegionGrowing(?!\+GrowCut)", "PUBLIC ITK|QuadEdgeMesh+RegionGrowing+GrowCut"
    if ($segmentationCMakeUpdated -ne $segmentationCMakeContent) {
        Write-Host "[Externals] Patching MITK Segmentation dependencies for the XQ toolkit build."
        Set-Content -LiteralPath $segmentationCMake -Value $segmentationCMakeUpdated
    }

    $segmentationFiles = Join-Path $SourceDir "Modules\Segmentation\files.cmake"
    if (-not (Test-Path -LiteralPath $segmentationFiles)) {
        throw "[Externals][ERROR] MITK Segmentation files.cmake not found: $segmentationFiles"
    }

    $monaiSources = @(
        "mitkMonaiLabelTool.cpp",
        "mitkMonaiLabel2DTool.cpp",
        "mitkMonaiLabel3DTool.cpp"
    )
    $segmentationFilesContent = Get-Content -Raw -LiteralPath $segmentationFiles
    $segmentationFilesUpdated = $segmentationFilesContent
    foreach ($monaiSource in $monaiSources) {
        $escapedSource = [regex]::Escape($monaiSource)
        $segmentationFilesUpdated = $segmentationFilesUpdated -replace "(?m)^\s+Interactions/$escapedSource\s*\r?\n", ""
    }
    if ($segmentationFilesUpdated -ne $segmentationFilesContent) {
        Write-Host "[Externals] Removing MITK MONAI Label sources from toolkit build."
        Set-Content -LiteralPath $segmentationFiles -Value $segmentationFilesUpdated
    }

    $multilabelCMake = Join-Path $SourceDir "Modules\Multilabel\CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $multilabelCMake)) {
        throw "[Externals][ERROR] MITK Multilabel CMakeLists.txt not found: $multilabelCMake"
    }

    $multilabelCMakeContent = Get-Content -Raw -LiteralPath $multilabelCMake
    $multilabelCMakeUpdated = $multilabelCMakeContent -replace "PACKAGE_DEPENDS ITK\|Smoothing(?!\+Review)", "PACKAGE_DEPENDS ITK|Smoothing+Review"
    if ($multilabelCMakeUpdated -ne $multilabelCMakeContent) {
        Write-Host "[Externals] Patching MITK Multilabel to request ITK Review for itkLabelGeometryImageFilter."
        Set-Content -LiteralPath $multilabelCMake -Value $multilabelCMakeUpdated
    }

    $itkPackageConfig = Join-Path $SourceDir "CMake\PackageDepends\MITK_ITK_Config.cmake"
    if (-not (Test-Path -LiteralPath $itkPackageConfig)) {
        throw "[Externals][ERROR] MITK ITK package config not found: $itkPackageConfig"
    }

    $itkPackageConfigContent = Get-Content -Raw -LiteralPath $itkPackageConfig
    $itkPackageConfigUpdated = $itkPackageConfigContent -replace 'if\(NOT itk_component MATCHES "\^ITK"\)', 'if(NOT itk_component MATCHES "^ITK" AND NOT itk_component STREQUAL "GrowCut")'
    if ($itkPackageConfigUpdated -ne $itkPackageConfigContent) {
        Write-Host "[Externals] Patching MITK ITK package config for remote GrowCut component naming."
        Set-Content -LiteralPath $itkPackageConfig -Value $itkPackageConfigUpdated
    }

    $installRules = Join-Path $SourceDir "CMake\mitkInstallRules.cmake"
    if (-not (Test-Path -LiteralPath $installRules)) {
        throw "[Externals][ERROR] MITK install rules not found: $installRules"
    }

    $installRulesContent = Get-Content -Raw -LiteralPath $installRules
    $installRulesUpdated = $installRulesContent
    $legacyVistaStyleInstall = '    MITK_INSTALL(FILES "${_qmake_path}/../plugins/styles/qwindowsvistastyle.dll")'
    $modernWindowsStyleInstall = @'
    if(EXISTS "${_qmake_path}/../plugins/styles/qwindowsvistastyle.dll")
      MITK_INSTALL(FILES "${_qmake_path}/../plugins/styles/qwindowsvistastyle.dll")
    elseif(EXISTS "${_qmake_path}/../plugins/styles/qmodernwindowsstyle.dll")
      MITK_INSTALL(FILES "${_qmake_path}/../plugins/styles/qmodernwindowsstyle.dll")
    endif()
'@
    if ($installRulesUpdated.Contains($legacyVistaStyleInstall) -and -not $installRulesUpdated.Contains('qmodernwindowsstyle.dll')) {
        Write-Host "[Externals] Patching MITK Qt Windows style install rule for Qt 6.7."
        $installRulesUpdated = $installRulesUpdated.Replace($legacyVistaStyleInstall, $modernWindowsStyleInstall)
    }

    $legacyWebEngineProcessInstall = '      MITK_INSTALL(PROGRAMS "${_qmake_path}/QtWebEngineProcess.exe")'
    $guardedWebEngineProcessInstall = @'
      if(EXISTS "${_qmake_path}/QtWebEngineProcess.exe")
        MITK_INSTALL(PROGRAMS "${_qmake_path}/QtWebEngineProcess.exe")
      endif()
'@
    if ($installRulesUpdated.Contains($legacyWebEngineProcessInstall) -and -not $installRulesUpdated.Contains('if(EXISTS "${_qmake_path}/QtWebEngineProcess.exe")')) {
        Write-Host "[Externals] Patching MITK Qt WebEngine process install rule for toolkit build without Qt WebEngine."
        $installRulesUpdated = $installRulesUpdated.Replace($legacyWebEngineProcessInstall, $guardedWebEngineProcessInstall)
    }

    if ($installRulesUpdated -ne $installRulesContent) {
        Set-Content -LiteralPath $installRules -Value $installRulesUpdated
    }
}

function Update-XQMitkRuntimeSearchPaths {
    param([Parameter(Mandatory)][string]$SourceDir)

    $searchPaths = Join-Path $SourceDir "CMake\mitkFunctionGetLibrarySearchPaths.cmake"
    if (-not (Test-Path -LiteralPath $searchPaths)) {
        throw "[Externals][ERROR] MITK library search path helper not found: $searchPaths"
    }

    $content = Get-Content -Raw -LiteralPath $searchPaths
    $anchor = @'
  if(WIN32)
    list(APPEND _dir_candidates "${ITK_DIR}/bin")
  endif()
'@
    $replacement = @'
  if(WIN32)
    foreach(_xq_external_dir_var ITK_DIR VTK_DIR GDCM_DIR HDF5_DIR)
      if(DEFINED ${_xq_external_dir_var})
        set(_xq_external_dir "${${_xq_external_dir_var}}")
        get_filename_component(_xq_external_root "${_xq_external_dir}" DIRECTORY)
        while(_xq_external_root AND NOT EXISTS "${_xq_external_root}/bin")
          get_filename_component(_xq_next_external_root "${_xq_external_root}" DIRECTORY)
          if("${_xq_next_external_root}" STREQUAL "${_xq_external_root}")
            break()
          endif()
          set(_xq_external_root "${_xq_next_external_root}")
        endwhile()
        if(EXISTS "${_xq_external_root}/bin")
          list(APPEND _dir_candidates "${_xq_external_root}/bin")
        endif()
      endif()
    endforeach()
  endif()
'@

    if ($content.Contains($anchor) -and -not $content.Contains("_xq_external_dir_var ITK_DIR VTK_DIR GDCM_DIR HDF5_DIR")) {
        Write-Host "[Externals] Patching MITK runtime library search paths for external ITK/VTK/GDCM/HDF5 installs."
        Set-Content -LiteralPath $searchPaths -Value $content.Replace($anchor, $replacement)
    }
}

function Set-XQMitkPluginListDefaults {
    param(
        [Parameter(Mandatory)][string]$SourceDir,
        [Parameter(Mandatory)][string[]]$PluginNames
    )

    $pluginList = Join-Path $SourceDir "Plugins\PluginList.cmake"
    if (-not (Test-Path -LiteralPath $pluginList)) {
        throw "[Externals][ERROR] MITK PluginList.cmake not found: $pluginList"
    }

    $content = Get-Content -Raw -LiteralPath $pluginList
    $updated = $content
    foreach ($plugin in $PluginNames) {
        $escapedPlugin = [regex]::Escape($plugin)
        $pattern = "(?m)^\s+{0}:(ON|OFF)\s*$" -f $escapedPlugin
        if ($updated -notmatch $pattern) {
            throw "[Externals][ERROR] MITK plugin not found in PluginList.cmake: $plugin"
        }
        $updated = $updated -replace $pattern, ("  {0}:ON" -f $plugin)
    }

    if ($updated -ne $content) {
        Write-Host "[Externals] Enabling required MITK BlueBerry Workbench plugins in PluginList.cmake."
        Set-Content -LiteralPath $pluginList -Value $updated
    }
}

function Update-XQMitkMeasurementToolboxForNoWebEngine {
    param([Parameter(Mandatory)][string]$SourceDir)

    $pluginDir = Join-Path $SourceDir "Plugins\org.mitk.gui.qt.measurementtoolbox"
    if (-not (Test-Path -LiteralPath $pluginDir -PathType Container)) {
        throw "[Externals][ERROR] MITK measurement toolbox plugin not found: $pluginDir"
    }

    $cmakeLists = Join-Path $pluginDir "CMakeLists.txt"
    $cmakeContent = Get-Content -Raw -LiteralPath $cmakeLists
    $legacyDependencyLine = "  MODULE_DEPENDS MitkQtWidgetsExt MitkImageStatistics MitkImageStatisticsUI MitkPlanarFigure MitkChart"
    if ($cmakeContent.Contains($legacyDependencyLine)) {
        Write-Host "[Externals] Removing MITK Measurement Toolbox ImageStatisticsUI/Chart dependency for the WebEngine-free BlueBerry build."
        $cmakeContent = $cmakeContent.Replace($legacyDependencyLine, "  MODULE_DEPENDS MitkQtWidgetsExt MitkPlanarFigure")
        Set-Content -LiteralPath $cmakeLists -Value $cmakeContent
    }

    $filesCmake = Join-Path $pluginDir "files.cmake"
    $filesContent = Get-Content -Raw -LiteralPath $filesCmake
    $filesUpdated = $filesContent
    foreach ($entry in @(
        "QmitkImageStatisticsView.cpp",
        "QmitkImageStatisticsViewControls.ui",
        "QmitkImageStatisticsView.h",
        "src/internal/QmitkImageStatisticsViewControls.ui",
        "src/internal/QmitkImageStatisticsView.h",
        "resources/QmitkImageStatisticsView.qrc"
    )) {
        $escapedEntry = [regex]::Escape($entry)
        $filesUpdated = $filesUpdated -replace "(?m)^\s+$escapedEntry\s*\r?\n", ""
    }
    if ($filesUpdated -ne $filesContent) {
        Write-Host "[Externals] Removing MITK Measurement Toolbox Image Statistics sources from BlueBerry build."
        Set-Content -LiteralPath $filesCmake -Value $filesUpdated
    }

    $activator = Join-Path $pluginDir "src\internal\mitkPluginActivator.cpp"
    $activatorContent = Get-Content -Raw -LiteralPath $activator
    $activatorUpdated = $activatorContent `
        -replace "(?m)^\s*#include ""QmitkImageStatisticsView\.h""\s*\r?\n", "" `
        -replace "(?m)^\s*BERRY_REGISTER_EXTENSION_CLASS\(QmitkImageStatisticsView,\s*context\)\s*\r?\n", ""
    if ($activatorUpdated -ne $activatorContent) {
        Write-Host "[Externals] Removing MITK Measurement Toolbox Image Statistics activator registration."
        Set-Content -LiteralPath $activator -Value $activatorUpdated
    }

    $pluginXml = Join-Path $pluginDir "plugin.xml"
    $pluginXmlContent = Get-Content -Raw -LiteralPath $pluginXml
    $pluginXmlUpdated = $pluginXmlContent `
        -replace "(?s)\s*<extension point=""org\.blueberry\.ui\.views"">\s*<view id=""org\.mitk\.views\.imagestatistics"".*?</extension>", "" `
        -replace "(?m)^\s*<keyword .*?id=""org\.mitk\.views\.imagestatistics\.Keyword""/>\s*\r?\n", ""
    if ($pluginXmlUpdated -ne $pluginXmlContent) {
        Write-Host "[Externals] Removing MITK Measurement Toolbox Image Statistics extension from plugin.xml."
        Set-Content -LiteralPath $pluginXml -Value $pluginXmlUpdated
    }
}

function Update-XQMitkBlueBerryToolkitSource {
    param([Parameter(Mandatory)][string]$SourceDir)

    $whitelistDir = Join-Path $SourceDir "CMake\Whitelists"
    if (-not (Test-Path -LiteralPath $whitelistDir -PathType Container)) {
        throw "[Externals][ERROR] MITK whitelist directory not found: $whitelistDir"
    }

    $whitelistPath = Join-Path $whitelistDir "XQBlueBerry.cmake"
    $whitelist = @'
set(enabled_modules
  Log
  Core
  DataTypesExt
  Annotation
  LegacyGL
  AlgorithmsExt
  MapperExt
  DICOM
  DICOMQI
  SceneSerializationBase
  PlanarFigure
  ImageExtraction
  SceneSerialization
  GraphAlgorithms
  Multilabel
  ImageStatistics
  ContourModel
  SurfaceInterpolation
  BoundingShape
  Segmentation
  SegmentationUI
  QtWidgets
  QtWidgetsExt
  AppUtil
  LegacyIO
  IOExt
  ROI
)

set(enabled_plugins
  org.blueberry.core.runtime
  org.blueberry.core.commands
  org.blueberry.core.expressions
  org.blueberry.ui.qt
  org.mitk.core.services
  org.mitk.gui.common
  org.mitk.gui.qt.common
  org.mitk.gui.qt.application
  org.mitk.gui.qt.ext
  org.mitk.gui.qt.extapplication
  org.mitk.gui.qt.datamanager
  org.mitk.gui.qt.mitkworkbench.intro
  org.mitk.gui.qt.stdmultiwidgeteditor
  org.mitk.gui.qt.mxnmultiwidgeteditor
  org.mitk.gui.qt.dicombrowser
  org.mitk.gui.qt.imagenavigator
  org.mitk.gui.qt.measurementtoolbox
  org.mitk.gui.qt.properties
  org.mitk.gui.qt.segmentation
  org.mitk.gui.qt.volumevisualization
  org.mitk.gui.qt.moviemaker
  org.mitk.gui.qt.pointsetinteraction
  org.mitk.gui.qt.remeshing
  org.mitk.gui.qt.viewnavigator
  org.mitk.gui.qt.imagecropper
  org.mitk.gui.qt.pixelvalue
)
'@
    if ((-not (Test-Path -LiteralPath $whitelistPath)) -or
        ((Get-Content -Raw -LiteralPath $whitelistPath) -ne $whitelist)) {
        Write-Host "[Externals] Writing MITK XQBlueBerry whitelist."
        Set-Content -LiteralPath $whitelistPath -Value $whitelist
    }

    Set-XQMitkPluginListDefaults -SourceDir $SourceDir -PluginNames @(
        "org.mitk.gui.qt.stdmultiwidgeteditor",
        "org.mitk.gui.qt.mxnmultiwidgeteditor",
        "org.mitk.gui.qt.imagenavigator",
        "org.mitk.gui.qt.measurementtoolbox",
        "org.mitk.gui.qt.viewnavigator",
        "org.mitk.gui.qt.pixelvalue"
    )

    Update-XQMitkMeasurementToolboxForNoWebEngine -SourceDir $SourceDir

    $segmentationUiFiles = Join-Path $SourceDir "Modules\SegmentationUI\files.cmake"
    if (-not (Test-Path -LiteralPath $segmentationUiFiles)) {
        throw "[Externals][ERROR] MITK SegmentationUI files.cmake not found: $segmentationUiFiles"
    }

    $monaiUiEntries = @(
        "Qmitk/QmitkMonaiLabelToolGUI.cpp",
        "Qmitk/QmitkMonaiLabel2DToolGUI.cpp",
        "Qmitk/QmitkMonaiLabel3DToolGUI.cpp",
        "Qmitk/QmitkMonaiLabelToolGUI.h",
        "Qmitk/QmitkMonaiLabel2DToolGUI.h",
        "Qmitk/QmitkMonaiLabel3DToolGUI.h",
        "Qmitk/QmitkMonaiLabelToolGUIControls.ui"
    )
    $segmentationUiFilesContent = Get-Content -Raw -LiteralPath $segmentationUiFiles
    $segmentationUiFilesUpdated = $segmentationUiFilesContent
    foreach ($monaiUiEntry in $monaiUiEntries) {
        $escapedEntry = [regex]::Escape($monaiUiEntry)
        $segmentationUiFilesUpdated = $segmentationUiFilesUpdated -replace "(?m)^\s+$escapedEntry\s*\r?\n", ""
    }
    if ($segmentationUiFilesUpdated -ne $segmentationUiFilesContent) {
        Write-Host "[Externals] Removing MITK MONAI Label SegmentationUI sources from BlueBerry build."
        Set-Content -LiteralPath $segmentationUiFiles -Value $segmentationUiFilesUpdated
    }
}

Write-Host "[Externals] Building $Dependency $($Entry.Version)"
Write-Host "[Externals] Source: $SourceDir"
Write-Host "[Externals] Build:  $BuildDir"
Write-Host "[Externals] Install: $InstallDir"

switch ($Dependency) {
    "Qt" {
        if (-not (Test-Path -LiteralPath (Join-Path $SourceDir "configure.bat"))) {
            throw "[Externals][ERROR] Qt configure.bat not found in $SourceDir"
        }
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
        Push-Location $BuildDir
        try {
            & (Join-Path $SourceDir "configure.bat") `
                -prefix $InstallDir `
                -opensource `
                -confirm-license `
                -release `
                -nomake examples `
                -nomake tests `
                -no-feature-zstd `
                -skip qtdatavis3d `
                -skip qtquick3dphysics
            if ($LASTEXITCODE -ne 0) {
                throw "[Externals][ERROR] Qt configure failed"
            }
            if (-not (Invoke-XQConfigureOnlyGuard)) {
                & $CMake --build . --parallel $Jobs
                if ($LASTEXITCODE -ne 0) { throw "[Externals][ERROR] Qt build failed" }
                & $CMake --install .
                if ($LASTEXITCODE -ne 0) { throw "[Externals][ERROR] Qt install failed" }
            }
        }
        finally {
            Pop-Location
        }
    }
    "Python" {
        $buildBat = Join-Path $SourceDir "PCbuild\build.bat"
        if (-not (Test-Path -LiteralPath $buildBat)) {
            throw "[Externals][ERROR] CPython PCbuild\build.bat not found in $SourceDir"
        }
        if (Invoke-XQConfigureOnlyGuard) {
            break
        }
        & $buildBat -p x64 -c $BuildType
        if ($LASTEXITCODE -ne 0) {
            throw "[Externals][ERROR] CPython build failed"
        }
        $pcbuild = Join-Path $SourceDir "PCbuild\amd64"
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $InstallDir "include") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $InstallDir "libs") -Force | Out-Null
        Copy-Item -Path (Join-Path $SourceDir "Include\*") -Destination (Join-Path $InstallDir "include") -Recurse -Force
        Copy-Item -Path (Join-Path $SourceDir "PC\pyconfig.h") -Destination (Join-Path $InstallDir "include") -Force
        Copy-Item -Path (Join-Path $pcbuild "python*.exe") -Destination $InstallDir -Force
        Copy-Item -Path (Join-Path $pcbuild "python*.dll") -Destination $InstallDir -Force
        Copy-Item -Path (Join-Path $pcbuild "python*.lib") -Destination (Join-Path $InstallDir "libs") -Force
        if (Test-Path -LiteralPath (Join-Path $SourceDir "Lib")) {
            Copy-Item -Path (Join-Path $SourceDir "Lib") -Destination $InstallDir -Recurse -Force
        }
    }
    "HDF5" {
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-DHDF5_BUILD_CPP_LIB=ON",
            "-DHDF5_BUILD_HL_LIB=ON",
            "-DHDF5_BUILD_TOOLS=ON",
            "-DHDF5_BUILD_EXAMPLES=OFF",
            "-DBUILD_TESTING=OFF"
        )
    }
    "TinyXML2" {
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-Dtinyxml2_BUILD_TESTING=OFF"
        )
    }
    "FreeType" {
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-DFT_DISABLE_BROTLI=ON",
            "-DFT_DISABLE_BZIP2=ON",
            "-DFT_DISABLE_PNG=ON",
            "-DFT_DISABLE_HARFBUZZ=ON"
        )
    }
    "SWIG" {
        $parserSource = Join-Path $SourceDir "Source\CParse\parser.c"
        $parserHeader = Join-Path $SourceDir "Source\CParse\parser.h"
        $configureScript = Join-Path $SourceDir "configure"
        if (-not (Test-Path -LiteralPath $configureScript)) {
            throw "[Externals][ERROR] Official SWIG configure script not found in $SourceDir"
        }
        if (-not (Test-Path -LiteralPath $parserSource) -or -not (Test-Path -LiteralPath $parserHeader)) {
            throw "[Externals][ERROR] Official SWIG release parser.c/parser.h not found; fetch the release archive mirror instead of the generated-file-free git tag."
        }

        $wrapperDir = Join-Path $BuildDir "wrapper"
        $cmakeBuildDir = Join-Path $BuildDir "build"
        $configIncludeDir = Join-Path $wrapperDir "Source\Include"
        New-Item -ItemType Directory -Path $wrapperDir -Force | Out-Null
        New-Item -ItemType Directory -Path $configIncludeDir -Force | Out-Null
        New-Item -ItemType Directory -Path $cmakeBuildDir -Force | Out-Null
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null

        $makefileText = Get-Content -Raw -LiteralPath (Join-Path $SourceDir "Source\Makefile.in")
        $sourceMatch = [regex]::Match($makefileText, "(?ms)^eswig_SOURCES\s*=\s*(.*?)(?:\r?\n\r?\n)")
        if (-not $sourceMatch.Success) {
            throw "[Externals][ERROR] Unable to read SWIG source list from Source\Makefile.in"
        }

        $swigSources = @()
        $sourceBody = $sourceMatch.Groups[1].Value -replace "\\\r?\n", "`n"
        foreach ($item in ($sourceBody -split "\s+")) {
            $sourceItem = $item.Trim()
            if (-not $sourceItem) {
                continue
            }
            if ($sourceItem -eq "CParse/parser.y") {
                $sourceItem = "CParse/parser.c"
            }
            $swigSources += (Join-Path (Join-Path $SourceDir "Source") $sourceItem)
        }

        $swigLibForConfig = (Join-Path $InstallDir "Lib").Replace("\", "/")
        $config = @"
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define PACKAGE "swig"
#define PACKAGE_BUGREPORT "http://www.swig.org"
#define PACKAGE_NAME "swig"
#define PACKAGE_STRING "swig 3.0.12"
#define PACKAGE_TARNAME "swig"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "3.0.12"
#define SIZEOF_VOID_P 8
#define STDC_HEADERS 1
#define SWIG_CXX "MSVC"
#define SWIG_LIB "$swigLibForConfig"
#define SWIG_LIB_WIN_UNIX ""
#define SWIG_PLATFORM "windows-x64"
#define VERSION "3.0.12"
#define SWIG_LANG "-tcl"
#if defined(_MSC_VER)
# define _CRT_SECURE_NO_DEPRECATE
# define _CRT_SECURE_NO_WARNINGS
#endif
"@
        Set-Content -LiteralPath (Join-Path $configIncludeDir "swigconfig.h") -Value $config

        function ConvertTo-CMakePath {
            param([Parameter(Mandatory)][string]$Path)
            return $Path.Replace("\", "/")
        }

        $cmakeSourceDir = ConvertTo-CMakePath -Path $SourceDir
        $cmakeConfigIncludeDir = ConvertTo-CMakePath -Path $configIncludeDir
        $cmakeSources = ($swigSources | ForEach-Object { '    "' + (ConvertTo-CMakePath -Path $_) + '"' }) -join "`n"

        $cmakeLists = @"
cmake_minimum_required(VERSION 3.21)
project(XQ_SWIG_Official C CXX)

add_executable(swig
$cmakeSources
)

target_include_directories(swig PRIVATE
    "$cmakeConfigIncludeDir"
    "$cmakeSourceDir/Source/Include"
    "$cmakeSourceDir/Source/DOH"
    "$cmakeSourceDir/Source/CParse"
    "$cmakeSourceDir/Source/Preprocessor"
    "$cmakeSourceDir/Source/Swig"
    "$cmakeSourceDir/Source/Modules"
)

if(MSVC)
    target_compile_definitions(swig PRIVATE
        YY_NO_UNISTD_H
    )
    target_compile_options(swig PRIVATE
        /wd4018 /wd4065 /wd4101 /wd4244 /wd4267 /wd4305 /wd4309 /wd4996
    )
endif()

install(TARGETS swig RUNTIME DESTINATION bin)
install(DIRECTORY "$cmakeSourceDir/Lib/" DESTINATION Lib)
install(DIRECTORY "$cmakeSourceDir/Lib/" DESTINATION bin/Lib)
"@
        Set-Content -LiteralPath (Join-Path $wrapperDir "CMakeLists.txt") -Value $cmakeLists

        & $CMake -S $wrapperDir -B $cmakeBuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$BuildType" "-DCMAKE_INSTALL_PREFIX=$InstallDir"
        if ($LASTEXITCODE -ne 0) {
            throw "[Externals][ERROR] CMake configure failed for SWIG"
        }

        if (Invoke-XQConfigureOnlyGuard) {
            break
        }

        & $CMake --build $cmakeBuildDir --config $BuildType --parallel $Jobs
        if ($LASTEXITCODE -ne 0) {
            throw "[Externals][ERROR] SWIG build failed"
        }

        & $CMake --install $cmakeBuildDir --config $BuildType
        if ($LASTEXITCODE -ne 0) {
            throw "[Externals][ERROR] SWIG install failed"
        }
    }
    "MMG" {
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON",
            "-DBUILD_TESTING=OFF"
        )
        if (-not $ConfigureOnly) {
            $runtimeSourceDir = Join-Path $BuildDir "lib"
            $runtimeInstallDir = Join-Path $InstallDir "bin"
            New-Item -ItemType Directory -Path $runtimeInstallDir -Force | Out-Null
            foreach ($runtimeDll in @("mmg.dll", "mmg2d.dll", "mmgs.dll", "mmg3d.dll")) {
                $runtimeSource = Join-Path $runtimeSourceDir $runtimeDll
                if (-not (Test-Path -LiteralPath $runtimeSource)) {
                    throw "[Externals][ERROR] Expected MMG runtime DLL not found: $runtimeSource"
                }
                Copy-Item -LiteralPath $runtimeSource -Destination $runtimeInstallDir -Force
            }
        }
    }
    "GDCM" {
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-DGDCM_BUILD_SHARED_LIBS=ON",
            "-DGDCM_BUILD_TESTING=OFF",
            "-DGDCM_BUILD_DOCBOOK_MANPAGES=OFF",
            "-DGDCM_BUILD_APPLICATIONS=OFF"
        )
    }
    "VTK" {
        $qtDir = Join-Path (Get-XQExternalInstallPath -Name "Qt") "lib\cmake\Qt6"
        $pythonDir = Get-XQExternalInstallPath -Name "Python"
        $pythonExe = Join-Path $pythonDir "python.exe"
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-DVTK_BUILD_TESTING=OFF",
            "-DVTK_GROUP_ENABLE_Qt=YES",
            "-DVTK_MODULE_ENABLE_VTK_GUISupportQt=YES",
            "-DVTK_MODULE_ENABLE_VTK_GUISupportQtQuick=NO",
            "-DVTK_MODULE_ENABLE_VTK_RenderingQt=YES",
            "-DVTK_WRAP_PYTHON=ON",
            "-DQt6_DIR=$qtDir",
            "-DPython3_ROOT_DIR=$pythonDir",
            "-DPython3_EXECUTABLE=$pythonExe"
        )
    }
    "ITK" {
        $qtRoot = ConvertTo-XQCMakePath -Path (Get-XQExternalInstallPath -Name "Qt")
        $qt6Dir = ConvertTo-XQCMakePath -Path (Join-Path $qtRoot "lib\cmake\Qt6")
        $qtCmakeDir = ConvertTo-XQCMakePath -Path (Join-Path $qtRoot "lib\cmake")
        $vtkDir = ConvertTo-XQCMakePath -Path (Join-Path (Get-XQExternalInstallPath -Name "VTK") "lib\cmake\vtk-9.3")
        $gdcmDir = ConvertTo-XQCMakePath -Path (Join-Path (Get-XQExternalInstallPath -Name "GDCM") "lib\gdcm-3.0")
        $hdf5Dir = ConvertTo-XQCMakePath -Path (Join-Path (Get-XQExternalInstallPath -Name "HDF5") "cmake")
        $systemDrive = if ($env:SystemDrive) { $env:SystemDrive } else { "C:" }
        $shortRoot = Join-Path $systemDrive "xq-ext"
        $itkSourceDir = New-XQShortDirectoryJunction -Name "ITK source" -TargetDir $SourceDir -ShortPath (Join-Path $shortRoot "itk-src")
        $itkBuildDir = New-XQShortDirectoryJunction -Name "ITK build" -TargetDir $BuildDir -ShortPath (Join-Path $shortRoot "itk-bld") -CreateTarget
        Write-Host "[Externals] ITK short source: $itkSourceDir"
        Write-Host "[Externals] ITK short build:  $itkBuildDir"
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $itkSourceDir -BuildDir $itkBuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_SHARED_LIBS=ON",
            "-DBUILD_TESTING=OFF",
            "-DModule_ITKVtkGlue=ON",
            "-DModule_ITKReview=ON",
            "-DModule_GrowCut=ON",
            "-DVTK_DIR=$vtkDir",
            "-DITK_USE_SYSTEM_GDCM=ON",
            "-DGDCM_DIR=$gdcmDir",
            "-DITK_USE_SYSTEM_HDF5=ON",
            "-DHDF5_DIR=$hdf5Dir",
            "-DQt6_DIR=$qt6Dir",
            "-DCMAKE_PREFIX_PATH=$qtCmakeDir"
        )
    }
    "OpenCascade" {
        $freetypeRoot = Get-XQExternalInstallPath -Name "FreeType"
        Invoke-XQCMakeBuild -Name $Dependency -SourceDir $SourceDir -BuildDir $BuildDir -InstallDir $InstallDir -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs @(
            "-DBUILD_LIBRARY_TYPE=Shared",
            "-DINSTALL_DIR_LAYOUT=Unix",
            "-DBUILD_MODULE_Draw=OFF",
            "-DUSE_TK=OFF",
            "-DUSE_VTK=OFF",
            "-DUSE_FREETYPE=ON",
            "-D3RDPARTY_FREETYPE_DIR=$freetypeRoot",
            "-U3RDPARTY_TCL_*",
            "-U3RDPARTY_TK_*"
        )
    }
    "MITK" {
        $qtRoot = Get-XQExternalInstallPath -Name "Qt"
        $pythonRoot = Get-XQExternalInstallPath -Name "Python"
        $mitkPrefix = ConvertTo-XQCMakePath -Path $InstallDir
        $gdcmRoot = Get-XQExternalInstallPath -Name "GDCM"
        $itkRoot = Get-XQExternalInstallPath -Name "ITK"
        $vtkRoot = Get-XQExternalInstallPath -Name "VTK"
        $hdf5Root = Get-XQExternalInstallPath -Name "HDF5"
        $swigRoot = Get-XQExternalInstallPath -Name "SWIG"
        $gdcmDir = ConvertTo-XQCMakePath -Path (Join-Path $gdcmRoot "lib\gdcm-3.0")
        $itkDir = ConvertTo-XQCMakePath -Path (Join-Path $itkRoot "lib\cmake\ITK-5.4")
        $vtkDir = ConvertTo-XQCMakePath -Path (Join-Path $vtkRoot "lib\cmake\vtk-9.3")
        $hdf5Dir = ConvertTo-XQCMakePath -Path (Join-Path $hdf5Root "cmake")
        $gdcmRoot = ConvertTo-XQCMakePath -Path $gdcmRoot
        $itkRoot = ConvertTo-XQCMakePath -Path $itkRoot
        $vtkRoot = ConvertTo-XQCMakePath -Path $vtkRoot
        $hdf5Root = ConvertTo-XQCMakePath -Path $hdf5Root
        $swigRoot = ConvertTo-XQCMakePath -Path $swigRoot
        $swigExe = ConvertTo-XQCMakePath -Path (Join-Path (Get-XQExternalInstallPath -Name "SWIG") "bin\swig.exe")
        $pythonExe = ConvertTo-XQCMakePath -Path (Join-Path $pythonRoot "python.exe")
        $pythonInclude = ConvertTo-XQCMakePath -Path (Join-Path $pythonRoot "include")
        $qt6Dir = ConvertTo-XQCMakePath -Path (Join-Path $qtRoot "lib\cmake\Qt6")
        $qtCmakeDir = ConvertTo-XQCMakePath -Path (Join-Path $qtRoot "lib\cmake")
        $rcToolPath = Get-XQWindowsSdkToolPath -ToolName "rc.exe"
        $mtToolPath = Get-XQWindowsSdkToolPath -ToolName "mt.exe"
        $windowsSdkToolDir = Split-Path -Parent $rcToolPath
        $rcExe = ConvertTo-XQCMakePath -Path $rcToolPath
        $mtExe = ConvertTo-XQCMakePath -Path $mtToolPath
        Update-XQMitkExternalProjectGitUpdates -SourceDir $SourceDir
        Update-XQMitkSuperbuildWindowsTools -SourceDir $SourceDir
        Update-XQMitkSuperbuildExternalDependencyDirs -SourceDir $SourceDir
        Update-XQMitkCtkSourcePatch -SourceDir $SourceDir
        Update-XQMitkQtComponentsForToolkitBuild -SourceDir $SourceDir
        Update-XQMitkToolkitSource -SourceDir $SourceDir
        Update-XQMitkRuntimeSearchPaths -SourceDir $SourceDir
        if ($Profile -eq "xq-blueberry") {
            Update-XQMitkBlueBerryToolkitSource -SourceDir $SourceDir
        }
        $systemDrive = if ($env:SystemDrive) { $env:SystemDrive } else { "C:" }
        $shortRoot = Join-Path $systemDrive "xq-ext"
        $shortSourceName = if ($Profile -eq "xq-blueberry") { "mitk-bb-src" } else { "mitk-src" }
        $shortBuildName = if ($Profile -eq "xq-blueberry") { "mitk-bb-bld" } else { "mitk-bld" }
        $mitkSourceDir = New-XQShortDirectoryJunction -Name "MITK source" -TargetDir $SourceDir -ShortPath (Join-Path $shortRoot $shortSourceName)
        $mitkBuildDir = New-XQShortDirectoryJunction -Name "MITK build" -TargetDir $BuildDir -ShortPath (Join-Path $shortRoot $shortBuildName) -CreateTarget
        $mitkSourceDir = ConvertTo-XQCMakePath -Path $mitkSourceDir
        $mitkBuildDir = ConvertTo-XQCMakePath -Path $mitkBuildDir
        Write-Host "[Externals] MITK short source: $mitkSourceDir"
        Write-Host "[Externals] MITK short build:  $mitkBuildDir"
        $mitkProfileArgs = if ($Profile -eq "xq-blueberry") {
            @(
                "-DMITK_USE_BLUEBERRY=ON",
                "-DMITK_WHITELIST=XQBlueBerry",
                "-DMITK_BUILD_org.mitk.gui.qt.stdmultiwidgeteditor=ON",
                "-DMITK_BUILD_org.mitk.gui.qt.mxnmultiwidgeteditor=ON",
                "-DMITK_BUILD_org.mitk.gui.qt.imagenavigator=ON",
                "-DMITK_BUILD_org.mitk.gui.qt.measurementtoolbox=ON",
                "-DMITK_BUILD_org.mitk.gui.qt.viewnavigator=ON",
                "-DMITK_BUILD_org.mitk.gui.qt.pixelvalue=ON"
            )
        }
        else {
            @(
                "-DMITK_USE_BLUEBERRY=OFF",
                "-DMITK_WHITELIST=XQToolkit"
            )
        }
        $hadGitSslBackend = Test-Path Env:GIT_SSL_BACKEND
        $hadGitConfigCount = Test-Path Env:GIT_CONFIG_COUNT
        $hadGitConfigKey0 = Test-Path Env:GIT_CONFIG_KEY_0
        $hadGitConfigValue0 = Test-Path Env:GIT_CONFIG_VALUE_0
        $hadPath = Test-Path Env:Path
        $previousGitSslBackend = $env:GIT_SSL_BACKEND
        $previousGitConfigCount = $env:GIT_CONFIG_COUNT
        $previousGitConfigKey0 = $env:GIT_CONFIG_KEY_0
        $previousGitConfigValue0 = $env:GIT_CONFIG_VALUE_0
        $previousPath = $env:Path
        try {
            $env:GIT_SSL_BACKEND = "openssl"
            $env:GIT_CONFIG_COUNT = "1"
            $env:GIT_CONFIG_KEY_0 = "http.sslBackend"
            $env:GIT_CONFIG_VALUE_0 = "openssl"
            $gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($gitCommand) {
                $gitRoot = Split-Path -Parent (Split-Path -Parent $gitCommand.Source)
                $gitUsrBin = Join-Path $gitRoot "usr\bin"
                if ((Test-Path -LiteralPath $gitUsrBin -PathType Container) -and
                    (($env:Path -split ';') -notcontains $gitUsrBin)) {
                    $env:Path = "$gitUsrBin;$env:Path"
                }
            }
            if ((Test-Path -LiteralPath $windowsSdkToolDir -PathType Container) -and
                (($env:Path -split ';') -notcontains $windowsSdkToolDir)) {
                $env:Path = "$windowsSdkToolDir;$env:Path"
            }
            $mitkConfigureArgs = @(
                "-DMITK_USE_SUPERBUILD=1",
                "-DMITK_BUILD_CONFIGURATION=Custom",
                "-DMITK_USE_GDCM=1",
                "-DMITK_BUILD_EXAMPLES=0",
                "-DMITK_BUILD_ALL_APPS=OFF",
                "-DBUILD_TESTING=0",
                "-DMITK_USE_CTK=ON",
                "-DMITK_USE_httplib=OFF",
                "-DMITK_USE_cpprestsdk=OFF",
                "-DMITK_USE_Python3=OFF",
                "-DMITK_USE_SWIG=OFF",
                "-DBUILD_SHARED_LIBS=1",
                "-DMITK_USE_Qt6=ON",
                "-DMITK_ADDITIONAL_C_FLAGS=/utf-8",
                "-DMITK_ADDITIONAL_CXX_FLAGS=/utf-8",
                "-DCMAKE_RC_COMPILER=$rcExe",
                "-DCMAKE_MT=$mtExe",
                "-DBLUEBERRY_USE_QT_HELP=OFF",
                "-DBLUEBERRY_QT_HELP_REQUIRED=OFF",
                "-DEXTERNAL_GDCM_DIR=$gdcmRoot",
                "-DEXTERNAL_ITK_DIR=$itkRoot",
                "-DEXTERNAL_VTK_DIR=$vtkRoot",
                "-DEXTERNAL_HDF5_DIR=$hdf5Root",
                "-DGDCM_DIR=$gdcmDir",
                "-DITK_DIR=$itkDir",
                "-DVTK_DIR=$vtkDir",
                "-DHDF5_DIR=$hdf5Dir",
                "-DSWIG_EXECUTABLE=$swigExe",
                "-DSWIG_DIR=$swigRoot",
                "-D_Python3_EXECUTABLE=$pythonExe",
                "-D_Python3_INCLUDE_DIR=$pythonInclude",
                "-DPython3_EXECUTABLE=$pythonExe",
                "-DPython3_INCLUDE_DIR=$pythonInclude",
                "-DPython3_ROOT_DIR=$pythonRoot",
                "-DQt6_DIR=$qt6Dir",
                "-DCMAKE_PREFIX_PATH=$itkRoot;$vtkRoot;$gdcmRoot;$qtCmakeDir",
                "-DCMAKE_OBJECT_PATH_MAX=1000"
            ) + $mitkProfileArgs
            Invoke-XQCMakeBuild -Name $Dependency -SourceDir $mitkSourceDir -BuildDir $mitkBuildDir -InstallDir $mitkPrefix -BuildType $BuildType -Jobs $Jobs -CMake $CMake -ConfigureOnly:$ConfigureOnly -ConfigureArgs $mitkConfigureArgs
        }
        finally {
            if ($hadGitSslBackend) { $env:GIT_SSL_BACKEND = $previousGitSslBackend } else { Remove-Item Env:GIT_SSL_BACKEND -ErrorAction SilentlyContinue }
            if ($hadGitConfigCount) { $env:GIT_CONFIG_COUNT = $previousGitConfigCount } else { Remove-Item Env:GIT_CONFIG_COUNT -ErrorAction SilentlyContinue }
            if ($hadGitConfigKey0) { $env:GIT_CONFIG_KEY_0 = $previousGitConfigKey0 } else { Remove-Item Env:GIT_CONFIG_KEY_0 -ErrorAction SilentlyContinue }
            if ($hadGitConfigValue0) { $env:GIT_CONFIG_VALUE_0 = $previousGitConfigValue0 } else { Remove-Item Env:GIT_CONFIG_VALUE_0 -ErrorAction SilentlyContinue }
            if ($hadPath) { $env:Path = $previousPath } else { Remove-Item Env:Path -ErrorAction SilentlyContinue }
        }
    }
    default {
        throw "[Externals][ERROR] No Windows build recipe is defined for $Dependency"
    }
}
