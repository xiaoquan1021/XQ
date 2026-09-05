@echo off
setlocal EnableExtensions DisableDelayedExpansion
call "%~dp0probes\vascular_foundation\configure_canonical_shell.bat" OFF
exit /b %errorlevel%
