@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Build-Native.ps1" %*
set "CODE=%ERRORLEVEL%"
if not "%CODE%"=="0" (
    echo Falha na compilacao. Confira as mensagens acima.
    pause
    exit /b %CODE%
)
echo Orion Memory v1.0.0 compilado com sucesso. Verifique a pasta dist.
pause
exit /b 0
