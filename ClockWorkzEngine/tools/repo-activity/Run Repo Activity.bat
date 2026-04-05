@echo off
setlocal

cd /d "%~dp0"

if exist "dist-current\RepoActivity\RepoActivity.exe" (
    "dist-current\RepoActivity\RepoActivity.exe"
) else if exist "dist\RepoActivity\RepoActivity.exe" (
    "dist\RepoActivity\RepoActivity.exe"
) else (
    where py >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        py -3 main.py
    ) else (
        python main.py
    )
)
