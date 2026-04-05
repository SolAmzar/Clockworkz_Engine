@echo off
setlocal

cd /d "%~dp0"

where py >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    py -3 -m PyInstaller --noconfirm --clean --onedir --windowed --name RepoActivity --distpath dist-current --workpath build-current --specpath . ..\main.py
) else (
    python -m PyInstaller --noconfirm --clean --onedir --windowed --name RepoActivity --distpath dist-current --workpath build-current --specpath . ..\main.py
)

echo.
if exist "dist-current\RepoActivity\RepoActivity.exe" (
    echo Build completed: "%cd%\dist-current\RepoActivity\RepoActivity.exe"
) else (
    echo Build failed.
)

pause
