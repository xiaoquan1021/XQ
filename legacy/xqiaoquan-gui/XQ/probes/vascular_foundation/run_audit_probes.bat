@echo off
setlocal EnableExtensions

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
for %%I in ("%SCRIPT_DIR%\..\..\..") do set "REPO_ROOT=%%~fI"

if not defined XQ_AUDIT_EXTERNALS_ROOT set "XQ_AUDIT_EXTERNALS_ROOT=%REPO_ROOT%\..\XIAOQUAN\Externals"
if not defined XQ_AUDIT_THICKNESS3D_SOURCE set "XQ_AUDIT_THICKNESS3D_SOURCE=%REPO_ROOT%\.trellis\workspace\ocean\itk-thickness3d-v5.3.0"
if not defined XQ_AUDIT_REAL_SURFACE set "XQ_AUDIT_REAL_SURFACE=%REPO_ROOT%\..\XIAOQUAN\0007_H_AO_H\Models\0090_0001.vtp"
if not defined XQ_AUDIT_BUILD_DIR set "XQ_AUDIT_BUILD_DIR=%REPO_ROOT%\XQ\build_vascular_foundation_probes"
if not defined XQ_AUDIT_VCVARS64 set "XQ_AUDIT_VCVARS64=C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined XQ_AUDIT_CMAKE set "XQ_AUDIT_CMAKE=C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined XQ_AUDIT_CTEST set "XQ_AUDIT_CTEST=C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"

set "XQ_AUDIT_INSTALL=%XQ_AUDIT_EXTERNALS_ROOT%\install\windows-x64"
set "XQ_AUDIT_VTK_DIR=%XQ_AUDIT_INSTALL%\vtk-9.3.0\lib\cmake\vtk-9.3"
set "XQ_AUDIT_ITK_DIR=%XQ_AUDIT_INSTALL%\itk-5.4.0\lib\cmake\ITK-5.4"
set "XQ_AUDIT_VMTK_SOURCE=%XQ_AUDIT_EXTERNALS_ROOT%\src\SimVascular\Code\ThirdParty\vmtk\simvascular_vmtk"
set "XQ_AUDIT_PREFIX=%XQ_AUDIT_INSTALL%;%XQ_AUDIT_INSTALL%\gdcm-3.0.10;%XQ_AUDIT_INSTALL%\hdf5-1.14.3;%XQ_AUDIT_INSTALL%\itk-5.4.0;%XQ_AUDIT_INSTALL%\qt-6.7.0;%XQ_AUDIT_INSTALL%\vtk-9.3.0"

call "%XQ_AUDIT_VCVARS64%" >nul
if errorlevel 1 exit /b %errorlevel%
set VSLANG=1033

"%XQ_AUDIT_CMAKE%" --fresh -S "%SCRIPT_DIR%" -B "%XQ_AUDIT_BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_PREFIX_PATH=%XQ_AUDIT_PREFIX%" ^
  "-DITK_DIR=%XQ_AUDIT_ITK_DIR%" ^
  "-DVTK_DIR=%XQ_AUDIT_VTK_DIR%" ^
  "-DXQ_AUDIT_EXPECTED_VTK_DIR=%XQ_AUDIT_VTK_DIR%" ^
  "-DXQ_AUDIT_THICKNESS3D_SOURCE_DIR=%XQ_AUDIT_THICKNESS3D_SOURCE%" ^
  "-DXQ_AUDIT_VMTK_SOURCE_DIR=%XQ_AUDIT_VMTK_SOURCE%" ^
  "-DXQ_AUDIT_REAL_SURFACE=%XQ_AUDIT_REAL_SURFACE%"
if errorlevel 1 exit /b %errorlevel%

"%XQ_AUDIT_CMAKE%" --build "%XQ_AUDIT_BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

"%XQ_AUDIT_CTEST%" --test-dir "%XQ_AUDIT_BUILD_DIR%" -C Release --output-on-failure
exit /b %errorlevel%
