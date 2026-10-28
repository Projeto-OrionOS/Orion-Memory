@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Build-Native.ps1" -Runtime win-x64 %*
set "BUILD_EXIT=%ERRORLEVEL%"
if not "%BUILD_EXIT%"=="0" (
    echo.
    echo Falha na compilacao do Orion Memory. Confira o erro acima.
    pause
)
exit /b %BUILD_EXIT%
