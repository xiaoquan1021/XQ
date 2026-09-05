@echo off
setlocal EnableExtensions DisableDelayedExpansion
call "%~dp0probes\vascular_foundation\configure_canonical_shell.bat" ON
exit /b %errorlevel%
