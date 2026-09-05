@echo off
setlocal EnableExtensions

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
for %%I in ("%SCRIPT_DIR%\..\..\..") do set "REPO_ROOT=%%~fI"

if not defined XQ_REMEDIATION_EXTERNALS_ROOT set "XQ_REMEDIATION_EXTERNALS_ROOT=%REPO_ROOT%\..\XIAOQUAN\Externals"
if not defined XQ_REMEDIATION_TEST_DATA_ROOT set "XQ_REMEDIATION_TEST_DATA_ROOT=%REPO_ROOT%\..\XIAOQUAN\0007_H_AO_H"
if not defined XQ_REMEDIATION_BUILD_DIR set "XQ_REMEDIATION_BUILD_DIR=%REPO_ROOT%\XQ\build_dependency_remediation"
if not defined XQ_REMEDIATION_QT_PLATFORM set "XQ_REMEDIATION_QT_PLATFORM=windows-x64-vascular"
if not defined XQ_REMEDIATION_QT_BUILD_DIR set "XQ_REMEDIATION_QT_BUILD_DIR=%XQ_REMEDIATION_EXTERNALS_ROOT%\build\%XQ_REMEDIATION_QT_PLATFORM%\QtBaseClean"
if not defined XQ_REMEDIATION_VCVARS64 set "XQ_REMEDIATION_VCVARS64=C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined XQ_REMEDIATION_CMAKE set "XQ_REMEDIATION_CMAKE=C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined XQ_REMEDIATION_CTEST set "XQ_REMEDIATION_CTEST=C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if not defined XQ_REMEDIATION_DUMPBIN set "XQ_REMEDIATION_DUMPBIN=C:\software\Visual Studio\Visual Studio2022\Community\VC\Tools\MSVC\14.43.34808\bin\Hostx64\x64\dumpbin.exe"

set "LEGACY_INSTALL=%XQ_REMEDIATION_EXTERNALS_ROOT%\install\windows-x64"
set "QT_ROOT=%XQ_REMEDIATION_EXTERNALS_ROOT%\install\%XQ_REMEDIATION_QT_PLATFORM%\qt-6.7.0"
set "LEGACY_QT_ROOT=%LEGACY_INSTALL%\qt-6.7.0"
set "VTK_ROOT=%LEGACY_INSTALL%\vtk-9.3.0"
set "ITK_ROOT=%LEGACY_INSTALL%\itk-5.4.0"
set "GDCM_ROOT=%LEGACY_INSTALL%\gdcm-3.0.10"
set "HDF5_ROOT=%LEGACY_INSTALL%\hdf5-1.14.3"
set "PYTHON_ROOT=%LEGACY_INSTALL%\python-3.11.0"
set "TINYXML2_ROOT=%LEGACY_INSTALL%\tinyxml2-8.0.0"
set "PREFIX=%QT_ROOT%;%GDCM_ROOT%;%HDF5_ROOT%;%ITK_ROOT%;%TINYXML2_ROOT%;%VTK_ROOT%"

call "%XQ_REMEDIATION_VCVARS64%" >nul
if errorlevel 1 exit /b %errorlevel%
set VSLANG=1033
set QT_QPA_PLATFORM=offscreen

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%SCRIPT_DIR%\check_isolated_qtbase.ps1" ^
  -BuildDir "%XQ_REMEDIATION_QT_BUILD_DIR%" ^
  -QtRoot "%QT_ROOT%" ^
  -LegacyQtRoot "%LEGACY_QT_ROOT%" ^
  -Dumpbin "%XQ_REMEDIATION_DUMPBIN%"
if errorlevel 1 exit /b %errorlevel%

"%XQ_REMEDIATION_CMAKE%" --fresh -S "%REPO_ROOT%\XQ" -B "%XQ_REMEDIATION_BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_PREFIX_PATH=%PREFIX%" ^
  "-DQt6_DIR=%QT_ROOT%\lib\cmake\Qt6" ^
  "-DVTK_DIR=%VTK_ROOT%\lib\cmake\vtk-9.3" ^
  "-DITK_DIR=%ITK_ROOT%\lib\cmake\ITK-5.4" ^
  "-Dtinyxml2_DIR=%TINYXML2_ROOT%\lib\cmake\tinyxml2" ^
  "-DPython3_ROOT_DIR=%PYTHON_ROOT%" ^
  "-DPython3_EXECUTABLE=%PYTHON_ROOT%\python.exe" ^
  "-DPython3_INCLUDE_DIR=%PYTHON_ROOT%\include" ^
  "-DPython3_LIBRARY=%PYTHON_ROOT%\libs\python311.lib" ^
  "-DXQ_TEST_DATA_ROOT=%XQ_REMEDIATION_TEST_DATA_ROOT%" ^
  -DXQ_DICOM_TEST_DATA_ROOT= ^
  -DXQ_ENABLE_TETGEN=OFF ^
  -DXQ_ENABLE_MMG=OFF
if errorlevel 1 exit /b %errorlevel%

"%XQ_REMEDIATION_CMAKE%" --build "%XQ_REMEDIATION_BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%SCRIPT_DIR%\check_dependency_baseline.ps1" ^
  -BuildDir "%XQ_REMEDIATION_BUILD_DIR%" ^
  -QtRoot "%QT_ROOT%" ^
  -LegacyQtRoot "%LEGACY_QT_ROOT%" ^
  -VtkRoot "%VTK_ROOT%" ^
  -ItkRoot "%ITK_ROOT%" ^
  -GdcmRoot "%GDCM_ROOT%" ^
  -Hdf5Root "%HDF5_ROOT%" ^
  -PythonRoot "%PYTHON_ROOT%" ^
  -TinyXml2Root "%TINYXML2_ROOT%" ^
  -Dumpbin "%XQ_REMEDIATION_DUMPBIN%"
if errorlevel 1 exit /b %errorlevel%

"%XQ_REMEDIATION_CTEST%" --test-dir "%XQ_REMEDIATION_BUILD_DIR%" -C Release --output-on-failure ^
  -R "^(test_dependency_baseline|test_arch_boundaries|test_itk_vascular_segmenter|test_dicom_series_adapter|test_dicom_fixture_policy|test_dicom_import_integration|test_app_startup|test_main_window)$"
if errorlevel 1 exit /b %errorlevel%

"%XQ_REMEDIATION_CTEST%" --test-dir "%XQ_REMEDIATION_BUILD_DIR%" -C Release --output-on-failure
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%SCRIPT_DIR%\run_dependency_negative_probes.ps1" ^
  -RepoRoot "%REPO_ROOT%" ^
  -CMake "%XQ_REMEDIATION_CMAKE%" ^
  -PrefixPath "%PREFIX%" ^
  -QtDir "%QT_ROOT%\lib\cmake\Qt6" ^
  -VtkDir "%VTK_ROOT%\lib\cmake\vtk-9.3" ^
  -ItkDir "%ITK_ROOT%\lib\cmake\ITK-5.4" ^
  -TinyXml2Dir "%TINYXML2_ROOT%\lib\cmake\tinyxml2" ^
  -PythonRoot "%PYTHON_ROOT%" ^
  -TestDataRoot "%XQ_REMEDIATION_TEST_DATA_ROOT%"
exit /b %errorlevel%
