@echo off
setlocal EnableExtensions

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
for %%I in ("%SCRIPT_DIR%\..\..\..") do set "REPO_ROOT=%%~fI"

if not defined XQ_AUDIT_EXTERNALS_ROOT set "XQ_AUDIT_EXTERNALS_ROOT=%REPO_ROOT%\..\XIAOQUAN\Externals"
if not defined XQ_AUDIT_TEST_DATA_ROOT set "XQ_AUDIT_TEST_DATA_ROOT=%REPO_ROOT%\..\XIAOQUAN\0007_H_AO_H"
if not defined XQ_AUDIT_MESH_BUILD_DIR set "XQ_AUDIT_MESH_BUILD_DIR=%REPO_ROOT%\XQ\build_dependency_audit"
if not defined XQ_AUDIT_QT_ROOT set "XQ_AUDIT_QT_ROOT=%XQ_AUDIT_EXTERNALS_ROOT%\install\windows-x64-vascular\qt-6.7.0"
if not defined XQ_AUDIT_VCVARS64 set "XQ_AUDIT_VCVARS64=C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined XQ_AUDIT_CMAKE set "XQ_AUDIT_CMAKE=C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined XQ_AUDIT_CTEST set "XQ_AUDIT_CTEST=C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if not defined XQ_AUDIT_DUMPBIN set "XQ_AUDIT_DUMPBIN=C:\software\Visual Studio\Visual Studio2022\Community\VC\Tools\MSVC\14.43.34808\bin\Hostx64\x64\dumpbin.exe"

set "XQ_AUDIT_INSTALL=%XQ_AUDIT_EXTERNALS_ROOT%\install\windows-x64"
if not defined XQ_AUDIT_PYTHON_ROOT set "XQ_AUDIT_PYTHON_ROOT=%XQ_AUDIT_INSTALL%\python-3.11.0"
set "XQ_AUDIT_PREFIX=%XQ_AUDIT_QT_ROOT%;%XQ_AUDIT_INSTALL%\gdcm-3.0.10;%XQ_AUDIT_INSTALL%\hdf5-1.14.3;%XQ_AUDIT_INSTALL%\itk-5.4.0;%XQ_AUDIT_INSTALL%\mmg-5.3.9;%XQ_AUDIT_INSTALL%\tinyxml2-8.0.0;%XQ_AUDIT_INSTALL%\vtk-9.3.0"

call "%XQ_AUDIT_VCVARS64%" >nul
if errorlevel 1 exit /b %errorlevel%
set VSLANG=1033
set QT_QPA_PLATFORM=offscreen

"%XQ_AUDIT_CMAKE%" --fresh -S "%REPO_ROOT%\XQ" -B "%XQ_AUDIT_MESH_BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_PREFIX_PATH=%XQ_AUDIT_PREFIX%" ^
  "-DQt6_DIR=%XQ_AUDIT_QT_ROOT%\lib\cmake\Qt6" ^
  "-DVTK_DIR=%XQ_AUDIT_INSTALL%\vtk-9.3.0\lib\cmake\vtk-9.3" ^
  "-DITK_DIR=%XQ_AUDIT_INSTALL%\itk-5.4.0\lib\cmake\ITK-5.4" ^
  "-Dtinyxml2_DIR=%XQ_AUDIT_INSTALL%\tinyxml2-8.0.0\lib\cmake\tinyxml2" ^
  "-DPython3_ROOT_DIR=%XQ_AUDIT_PYTHON_ROOT%" ^
  "-DPython3_EXECUTABLE=%XQ_AUDIT_PYTHON_ROOT%\python.exe" ^
  "-DPython3_INCLUDE_DIR=%XQ_AUDIT_PYTHON_ROOT%\include" ^
  "-DPython3_LIBRARY=%XQ_AUDIT_PYTHON_ROOT%\libs\python311.lib" ^
  "-DXQ_TEST_DATA_ROOT=%XQ_AUDIT_TEST_DATA_ROOT%" ^
  -DXQ_DICOM_TEST_DATA_ROOT= ^
  -DXQ_ENABLE_TETGEN=ON ^
  -DXQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY=ON ^
  -DXQ_ENABLE_MMG=ON
if errorlevel 1 exit /b %errorlevel%

"%XQ_AUDIT_CMAKE%" --build "%XQ_AUDIT_MESH_BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%SCRIPT_DIR%\check_dependency_baseline.ps1" ^
  -BuildDir "%XQ_AUDIT_MESH_BUILD_DIR%" ^
  -QtRoot "%XQ_AUDIT_QT_ROOT%" ^
  -LegacyQtRoot "%XQ_AUDIT_INSTALL%\qt-6.7.0" ^
  -VtkRoot "%XQ_AUDIT_INSTALL%\vtk-9.3.0" ^
  -ItkRoot "%XQ_AUDIT_INSTALL%\itk-5.4.0" ^
  -GdcmRoot "%XQ_AUDIT_INSTALL%\gdcm-3.0.10" ^
  -Hdf5Root "%XQ_AUDIT_INSTALL%\hdf5-1.14.3" ^
  -PythonRoot "%XQ_AUDIT_PYTHON_ROOT%" ^
  -TinyXml2Root "%XQ_AUDIT_INSTALL%\tinyxml2-8.0.0" ^
  -MmgRoot "%XQ_AUDIT_INSTALL%\mmg-5.3.9" ^
  -Dumpbin "%XQ_AUDIT_DUMPBIN%"
if errorlevel 1 exit /b %errorlevel%

"%XQ_AUDIT_CTEST%" --test-dir "%XQ_AUDIT_MESH_BUILD_DIR%" -C Release --output-on-failure ^
  -R "^(test_tetgen_volume_mesh|test_mmg_volume_mesh|test_arch_boundaries)$"
if errorlevel 1 exit /b %errorlevel%

"%XQ_AUDIT_CTEST%" --test-dir "%XQ_AUDIT_MESH_BUILD_DIR%" -C Release --output-on-failure
exit /b %errorlevel%
