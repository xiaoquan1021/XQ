@echo off
setlocal EnableExtensions DisableDelayedExpansion

call "%~dp0probes\vascular_foundation\canonical_shell_env.bat"
if errorlevel 1 exit /b 1

if not exist "%XQ_CANONICAL_VCVARS64%" (
    echo [XQ canonical shell] ERROR: missing vcvars64.bat: %XQ_CANONICAL_VCVARS64% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_CMAKE%" (
    echo [XQ canonical shell] ERROR: missing cmake.exe: %XQ_CANONICAL_CMAKE% 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_CTEST%" (
    echo [XQ canonical shell] ERROR: missing ctest.exe: %XQ_CANONICAL_CTEST% 1>&2
    exit /b 1
)

call "%XQ_CANONICAL_VCVARS64%" >nul
if errorlevel 1 (
    echo [XQ canonical shell] ERROR: vcvars64.bat failed. 1>&2
    exit /b 1
)
set "VSLANG=1033"
set "QT_QPA_PLATFORM=offscreen"
set "CTEST_PARALLEL_LEVEL=1"

if not defined XQ_CANONICAL_RUN_ID (
    for /f %%I in ('powershell.exe -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "XQ_CANONICAL_RUN_ID=%%I"
)
if not defined XQ_CANONICAL_RUN_ID (
    echo [XQ canonical shell] ERROR: could not create a verification run id. 1>&2
    exit /b 1
)

set "XQ_CANONICAL_RUN_LOG_DIR=%XQ_CANONICAL_LOG_ROOT%\%XQ_CANONICAL_RUN_ID%"
if exist "%XQ_CANONICAL_RUN_LOG_DIR%\" (
    echo [XQ canonical shell] ERROR: refusing to overwrite an existing evidence run: %XQ_CANONICAL_RUN_LOG_DIR% 1>&2
    exit /b 1
)
mkdir "%XQ_CANONICAL_RUN_LOG_DIR%"
if errorlevel 1 (
    echo [XQ canonical shell] ERROR: could not create log directory: %XQ_CANONICAL_RUN_LOG_DIR% 1>&2
    exit /b 1
)

> "%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo run_id=%XQ_CANONICAL_RUN_ID%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo computer=%COMPUTERNAME%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo user=%USERNAME%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo repo_root=%XQ_CANONICAL_REPO_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo sample_id=%XQ_CANONICAL_SAMPLE_ID%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo test_data_root=%XQ_CANONICAL_TEST_DATA_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo flow_on_build=%XQ_CANONICAL_BUILD_ON%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo flow_off_build=%XQ_CANONICAL_BUILD_OFF%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo qt_root=%XQ_CANONICAL_QT_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo vtk_root=%XQ_CANONICAL_VTK_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo itk_root=%XQ_CANONICAL_ITK_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo gdcm_root=%XQ_CANONICAL_GDCM_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo hdf5_root=%XQ_CANONICAL_HDF5_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo tinyxml2_root=%XQ_CANONICAL_TINYXML2_ROOT%
>>"%XQ_CANONICAL_RUN_LOG_DIR%\metadata.txt" echo configure_only_python_root=%XQ_CANONICAL_PYTHON_ROOT%

set "XQ_CANONICAL_FOCUSED_REGEX=^(test_dependency_baseline|test_arch_boundaries|test_itk_vascular_segmenter|test_dicom_series_adapter|test_dicom_fixture_policy|test_dicom_import_integration|test_app_startup|test_main_window)$"

echo [XQ canonical shell] Logs: %XQ_CANONICAL_RUN_LOG_DIR%

echo [XQ canonical shell] STEP 00 - source contract
set "XQ_CANONICAL_STEP_NAME=source contract"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\00-source-contract.log"
"%XQ_CANONICAL_CMAKE%" -DXQ_SOURCE_DIR="%XQ_CANONICAL_XQ_SOURCE_DIR%" -P "%XQ_CANONICAL_XQ_SOURCE_DIR%\tests\cmake\check_dependency_baseline.cmake" > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 01 - Flow ON configure, build, graph and PE closure
set "XQ_CANONICAL_STEP_NAME=Flow ON build"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\01-flow-on-build.log"
call "%XQ_CANONICAL_SCRIPT_DIR%\configure_canonical_shell.bat" ON > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 02 - Flow ON focused dependency/startup evidence
set "XQ_CANONICAL_STEP_NAME=Flow ON focused CTest"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\02-flow-on-focused.log"
"%XQ_CANONICAL_CTEST%" --test-dir "%XQ_CANONICAL_BUILD_ON%" -C Release --output-on-failure -R "%XQ_CANONICAL_FOCUSED_REGEX%" > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 03 - Flow ON full regression evidence
set "XQ_CANONICAL_STEP_NAME=Flow ON full CTest"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\03-flow-on-full.log"
"%XQ_CANONICAL_CTEST%" --test-dir "%XQ_CANONICAL_BUILD_ON%" -C Release --output-on-failure > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 04 - Flow OFF configure, build, graph and PE closure
set "XQ_CANONICAL_STEP_NAME=Flow OFF build"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\04-flow-off-build.log"
call "%XQ_CANONICAL_SCRIPT_DIR%\configure_canonical_shell.bat" OFF > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 05 - Flow OFF focused dependency/startup evidence
set "XQ_CANONICAL_STEP_NAME=Flow OFF focused CTest"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\05-flow-off-focused.log"
"%XQ_CANONICAL_CTEST%" --test-dir "%XQ_CANONICAL_BUILD_OFF%" -C Release --output-on-failure -R "%XQ_CANONICAL_FOCUSED_REGEX%" > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 06 - Flow OFF full regression evidence
set "XQ_CANONICAL_STEP_NAME=Flow OFF full CTest"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\06-flow-off-full.log"
"%XQ_CANONICAL_CTEST%" --test-dir "%XQ_CANONICAL_BUILD_OFF%" -C Release --output-on-failure > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] STEP 07 - explicit-package negative evidence
set "XQ_CANONICAL_STEP_NAME=negative probes"
set "XQ_CANONICAL_STEP_LOG=%XQ_CANONICAL_RUN_LOG_DIR%\07-negative-probes.log"
powershell.exe -NoProfile -ExecutionPolicy Bypass ^
  -File "%XQ_CANONICAL_SCRIPT_DIR%\run_dependency_negative_probes.ps1" ^
  -RepoRoot "%XQ_CANONICAL_REPO_ROOT%" ^
  -CMake "%XQ_CANONICAL_CMAKE%" ^
  -PrefixPath "%XQ_CANONICAL_PREFIX_PATH%" ^
  -QtDir "%XQ_CANONICAL_QT_DIR%" ^
  -VtkDir "%XQ_CANONICAL_VTK_DIR%" ^
  -ItkDir "%XQ_CANONICAL_ITK_DIR%" ^
  -TinyXml2Dir "%XQ_CANONICAL_TINYXML2_DIR%" ^
  -PythonRoot "%XQ_CANONICAL_PYTHON_ROOT%" ^
  -TestDataRoot "%XQ_CANONICAL_TEST_DATA_ROOT%" > "%XQ_CANONICAL_STEP_LOG%" 2>&1
set "XQ_CANONICAL_STEP_EXIT=%errorlevel%"
type "%XQ_CANONICAL_STEP_LOG%"
if not "%XQ_CANONICAL_STEP_EXIT%"=="0" (
    echo [XQ canonical shell] ERROR: %XQ_CANONICAL_STEP_NAME% failed with exit %XQ_CANONICAL_STEP_EXIT%. 1>&2
    echo [XQ canonical shell] Evidence log: %XQ_CANONICAL_STEP_LOG% 1>&2
    exit /b 1
)
echo [XQ canonical shell] PASS-EVIDENCE: %XQ_CANONICAL_STEP_NAME%

echo [XQ canonical shell] Automated evidence completed: %XQ_CANONICAL_RUN_LOG_DIR%
echo [XQ canonical shell] Manual GUI and requirement acceptance are still pending.
exit /b 0
