@echo off
chcp 65001 >nul
mode con: cols=77 lines=28
cls

:: Enable ANSI escape codes for colors
echo|set /p="["

echo [30;1mBuilding Symphony...[0m
echo.

premake5.exe vs2019

echo.
echo [33;1mPress any key to continue...[0m
pause >nul
