@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" (
    echo Usage: %~nx0 ^<ON^|OFF^> 1>&2
    exit /b 2
)
if not "%~2"=="" (
    echo Usage: %~nx0 ^<ON^|OFF^> 1>&2
    exit /b 2
)
set "XQ_CANONICAL_FLOW_MODE="
if /I "%~1"=="ON" (
    set "XQ_CANONICAL_FLOW_MODE=ON"
)
if /I "%~1"=="OFF" (
    set "XQ_CANONICAL_FLOW_MODE=OFF"
)
if not defined XQ_CANONICAL_FLOW_MODE (
    echo Usage: %~nx0 ^<ON^|OFF^> 1>&2
    exit /b 2
)

call "%~dp0canonical_shell_env.bat"
if errorlevel 1 exit /b 1

if not exist "%XQ_CANONICAL_XQ_SOURCE_DIR%\CMakeLists.txt" (
    echo [XQ canonical shell] ERROR: missing XQ source CMakeLists: %XQ_CANONICAL_XQ_SOURCE_DIR%\CMakeLists.txt 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_TEST_DATA_ROOT%\" (
    echo [XQ canonical shell] ERROR: missing test data root: %XQ_CANONICAL_TEST_DATA_ROOT% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_QT_BUILD_DIR%\CMakeCache.txt" (
    echo [XQ canonical shell] ERROR: missing isolated QtBase cache: %XQ_CANONICAL_QT_BUILD_DIR%\CMakeCache.txt 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_QT_DIR%\Qt6Config.cmake" (
    echo [XQ canonical shell] ERROR: missing Qt 6.7.0 package: %XQ_CANONICAL_QT_DIR%\Qt6Config.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_LEGACY_QT_ROOT%\bin\Qt6Core.dll" (
    echo [XQ canonical shell] ERROR: missing legacy Qt identity witness: %XQ_CANONICAL_LEGACY_QT_ROOT%\bin\Qt6Core.dll 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_VTK_DIR%\vtk-config.cmake" (
    echo [XQ canonical shell] ERROR: missing VTK 9.3.0 package: %XQ_CANONICAL_VTK_DIR%\vtk-config.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_ITK_DIR%\ITKConfig.cmake" (
    echo [XQ canonical shell] ERROR: missing ITK 5.4.0 package: %XQ_CANONICAL_ITK_DIR%\ITKConfig.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_GDCM_DIR%\GDCMConfig.cmake" (
    echo [XQ canonical shell] ERROR: missing GDCM 3.0.10 package: %XQ_CANONICAL_GDCM_DIR%\GDCMConfig.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_GDCM_DIR%\GDCMConfigVersion.cmake" (
    echo [XQ canonical shell] ERROR: missing GDCM 3.0.10 version metadata: %XQ_CANONICAL_GDCM_DIR%\GDCMConfigVersion.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_HDF5_DIR%\hdf5-config.cmake" (
    echo [XQ canonical shell] ERROR: missing HDF5 1.14.3 package: %XQ_CANONICAL_HDF5_DIR%\hdf5-config.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_HDF5_DIR%\hdf5-config-version.cmake" (
    echo [XQ canonical shell] ERROR: missing HDF5 1.14.3 version metadata: %XQ_CANONICAL_HDF5_DIR%\hdf5-config-version.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_TINYXML2_DIR%\tinyxml2Config.cmake" (
    echo [XQ canonical shell] ERROR: missing tinyxml2 8.0.0 package: %XQ_CANONICAL_TINYXML2_DIR%\tinyxml2Config.cmake 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_PYTHON_EXE%" (
    echo [XQ canonical shell] ERROR: missing configure-only Python 3.11 executable: %XQ_CANONICAL_PYTHON_EXE% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_PYTHON_INCLUDE%\" (
    echo [XQ canonical shell] ERROR: missing configure-only Python 3.11 include: %XQ_CANONICAL_PYTHON_INCLUDE% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_PYTHON_LIBRARY%" (
    echo [XQ canonical shell] ERROR: missing configure-only Python 3.11 import library: %XQ_CANONICAL_PYTHON_LIBRARY% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_VCVARS64%" (
    echo [XQ canonical shell] ERROR: missing vcvars64.bat: %XQ_CANONICAL_VCVARS64% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_CMAKE%" (
    echo [XQ canonical shell] ERROR: missing cmake.exe: %XQ_CANONICAL_CMAKE% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_DUMPBIN%" (
    echo [XQ canonical shell] ERROR: missing dumpbin.exe: %XQ_CANONICAL_DUMPBIN% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_SCRIPT_DIR%\check_isolated_qtbase.ps1" (
    echo [XQ canonical shell] ERROR: missing isolated Qt checker. 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_SCRIPT_DIR%\check_dependency_baseline.ps1" (
    echo [XQ canonical shell] ERROR: missing dependency baseline checker. 1>&2
    exit /b 1
)

if /I "%XQ_CANONICAL_FLOW_MODE%"=="ON" (
    set "XQ_CANONICAL_BUILD_DIR=%XQ_CANONICAL_BUILD_ON%"
) else (
    set "XQ_CANONICAL_BUILD_DIR=%XQ_CANONICAL_BUILD_OFF%"
)

call "%XQ_CANONICAL_VCVARS64%" >nul
if errorlevel 1 (
    echo [XQ canonical shell] ERROR: vcvars64.bat failed. 1>&2
    exit /b 1
)
set "VSLANG=1033"
set "QT_QPA_PLATFORM=offscreen"
set "CMAKE_PREFIX_PATH="
set "CMAKE_FRAMEWORK_PATH="
set "CMAKE_APPBUNDLE_PATH="

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%XQ_CANONICAL_SCRIPT_DIR%\check_isolated_qtbase.ps1" ^
  -BuildDir "%XQ_CANONICAL_QT_BUILD_DIR%" ^
  -QtRoot "%XQ_CANONICAL_QT_ROOT%" ^
  -LegacyQtRoot "%XQ_CANONICAL_LEGACY_QT_ROOT%" ^
  -Dumpbin "%XQ_CANONICAL_DUMPBIN%"
if errorlevel 1 exit /b 1

"%XQ_CANONICAL_CMAKE%" --fresh -S "%XQ_CANONICAL_XQ_SOURCE_DIR%" -B "%XQ_CANONICAL_BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE ^
  -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE ^
  -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=TRUE ^
  "-DCMAKE_PREFIX_PATH=%XQ_CANONICAL_PREFIX_PATH%" ^
  "-DQt6_DIR=%XQ_CANONICAL_QT_DIR%" ^
  "-DVTK_DIR=%XQ_CANONICAL_VTK_DIR%" ^
  "-DITK_DIR=%XQ_CANONICAL_ITK_DIR%" ^
  "-Dtinyxml2_DIR=%XQ_CANONICAL_TINYXML2_DIR%" ^
  "-DPython3_ROOT_DIR=%XQ_CANONICAL_PYTHON_ROOT%" ^
  "-DPython3_EXECUTABLE=%XQ_CANONICAL_PYTHON_EXE%" ^
  "-DPython3_INCLUDE_DIR=%XQ_CANONICAL_PYTHON_INCLUDE%" ^
  "-DPython3_LIBRARY=%XQ_CANONICAL_PYTHON_LIBRARY%" ^
  "-DXQ_TEST_DATA_ROOT=%XQ_CANONICAL_TEST_DATA_ROOT%" ^
  -DXQ_DICOM_TEST_DATA_ROOT= ^
  "-DXQ_ENABLE_FLOW=%XQ_CANONICAL_FLOW_MODE%" ^
  -DXQ_ENABLE_TETGEN=OFF ^
  -DXQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY=OFF ^
  -DXQ_ENABLE_MMG=OFF
if errorlevel 1 exit /b 1

"%XQ_CANONICAL_CMAKE%" --build "%XQ_CANONICAL_BUILD_DIR%" --config Release --clean-first
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%XQ_CANONICAL_SCRIPT_DIR%\check_dependency_baseline.ps1" ^
  -BuildDir "%XQ_CANONICAL_BUILD_DIR%" ^
  -QtRoot "%XQ_CANONICAL_QT_ROOT%" ^
  -LegacyQtRoot "%XQ_CANONICAL_LEGACY_QT_ROOT%" ^
  -VtkRoot "%XQ_CANONICAL_VTK_ROOT%" ^
  -ItkRoot "%XQ_CANONICAL_ITK_ROOT%" ^
  -GdcmRoot "%XQ_CANONICAL_GDCM_ROOT%" ^
  -Hdf5Root "%XQ_CANONICAL_HDF5_ROOT%" ^
  -PythonRoot "%XQ_CANONICAL_PYTHON_ROOT%" ^
  -TinyXml2Root "%XQ_CANONICAL_TINYXML2_ROOT%" ^
  -Dumpbin "%XQ_CANONICAL_DUMPBIN%"
if errorlevel 1 exit /b 1

echo [XQ canonical shell] Flow %XQ_CANONICAL_FLOW_MODE% build is ready: %XQ_CANONICAL_BUILD_DIR%
exit /b 0
