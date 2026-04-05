@echo off
setlocal

cd /d "%~dp0"

echo Stopping running RepoActivity.exe...
taskkill /im RepoActivity.exe /f >nul 2>nul

echo Cleaning old RepoActivity build output...
if exist "build-current" rmdir /s /q "build-current"
if exist "dist-current" rmdir /s /q "dist-current"
if exist "RepoActivity.spec" del /q "RepoActivity.spec"

echo.
echo Building RepoActivity.exe...
where py >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    py -3 -m PyInstaller --noconfirm --clean --onedir --windowed --name RepoActivity --distpath dist-current --workpath build-current --specpath . ..\main.py
) else (
    python -m PyInstaller --noconfirm --clean --onedir --windowed --name RepoActivity --distpath dist-current --workpath build-current --specpath . ..\main.py
)

echo.
if exist "dist-current\RepoActivity\RepoActivity.exe" (
    echo Rebuild completed: "%cd%\dist-current\RepoActivity\RepoActivity.exe"
) else (
    echo Rebuild failed.
)

pause
