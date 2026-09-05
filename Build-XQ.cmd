@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\build-xq.ps1" -ExternalsRoot "%SCRIPT_DIR%..\Externals" %*
endlocal
