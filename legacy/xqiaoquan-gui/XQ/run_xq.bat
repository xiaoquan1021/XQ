@echo off
setlocal EnableExtensions DisableDelayedExpansion

call "%~dp0probes\vascular_foundation\canonical_shell_env.bat"
if errorlevel 1 exit /b 1

if not exist "%XQ_CANONICAL_APP_EXE%" (
    echo [XQ canonical shell] ERROR: canonical Flow ON app is missing: %XQ_CANONICAL_APP_EXE% 1>&2
    echo [XQ canonical shell] Run build_gui_wt.bat first. 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_QT_ROOT%\bin\Qt6Core.dll" (
    echo [XQ canonical shell] ERROR: isolated Qt runtime is missing: %XQ_CANONICAL_QT_ROOT%\bin\Qt6Core.dll 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_QT_PLUGIN_PATH%\platforms\qwindows.dll" (
    echo [XQ canonical shell] ERROR: isolated Qt windows plugin is missing: %XQ_CANONICAL_QT_PLUGIN_PATH%\platforms\qwindows.dll 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_VTK_ROOT%\bin\" (
    echo [XQ canonical shell] ERROR: locked VTK runtime bin is missing: %XQ_CANONICAL_VTK_ROOT%\bin 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_ITK_ROOT%\bin\" (
    echo [XQ canonical shell] ERROR: locked ITK runtime bin is missing: %XQ_CANONICAL_ITK_ROOT%\bin 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_GDCM_ROOT%\bin\" (
    echo [XQ canonical shell] ERROR: locked GDCM runtime bin is missing: %XQ_CANONICAL_GDCM_ROOT%\bin 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_HDF5_ROOT%\bin\" (
    echo [XQ canonical shell] ERROR: locked HDF5 runtime bin is missing: %XQ_CANONICAL_HDF5_ROOT%\bin 1>&2
    exit /b 1
)
if not exist "%XQ_CANONICAL_TINYXML2_ROOT%\bin\" (
    echo [XQ canonical shell] ERROR: locked tinyxml2 runtime bin is missing: %XQ_CANONICAL_TINYXML2_ROOT%\bin 1>&2
    exit /b 1
)

rem Do not inherit host Anaconda/Python/MMG paths into the product runtime.
set "PATH=%XQ_CANONICAL_RUNTIME_PATH%;%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem"
set "PYTHONHOME="
set "PYTHONPATH="
set "CONDA_PREFIX="
set "CONDA_DEFAULT_ENV="
set "CONDA_PROMPT_MODIFIER="
set "VIRTUAL_ENV="
set "XQ_CANONICAL_PYTHON_ROOT="
set "XQ_CANONICAL_PYTHON_EXE="
set "XQ_CANONICAL_PYTHON_INCLUDE="
set "XQ_CANONICAL_PYTHON_LIBRARY="
set "XQ_CANONICAL_LEGACY_QT_ROOT="
set "XQ_CANONICAL_QT_BUILD_DIR="
set "XQ_CANONICAL_VCVARS64="
set "XQ_CANONICAL_CMAKE="
set "XQ_CANONICAL_CTEST="
set "XQ_CANONICAL_DUMPBIN="
set "XQ_CANONICAL_TEST_DATA_ROOT="
set "XQ_CANONICAL_PREFIX_PATH="
set "QT_PLUGIN_PATH=%XQ_CANONICAL_QT_PLUGIN_PATH%"
if not defined XQ_CANONICAL_QPA_PLATFORM set "XQ_CANONICAL_QPA_PLATFORM=windows"
set "QT_QPA_PLATFORM=%XQ_CANONICAL_QPA_PLATFORM%"

echo [XQ canonical shell] Launching: %XQ_CANONICAL_APP_EXE%
"%XQ_CANONICAL_APP_EXE%" %*
set "XQ_CANONICAL_APP_EXIT=%errorlevel%"
exit /b %XQ_CANONICAL_APP_EXIT%
