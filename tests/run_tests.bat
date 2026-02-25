@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_tests.ps1 %*
exit /b %ERRORLEVEL%
