@echo off
REM Gera o .zip portatil. Argumentos opcionais: -QtDir C:\Qt\6.x.x\msvc2022_64 -Version 0.2.0 -NoCore
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" %*
pause
