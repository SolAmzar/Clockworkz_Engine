@echo off
setlocal

cd /d "%~dp0"

set "SCRIPT_DIR=%cd%"
set "SOURCE_DIR=%SCRIPT_DIR%\repo-activity"
set "ARTIFACT_DIR=%SOURCE_DIR%\_workspace-artifacts"
set "BUILD_DIR=%ARTIFACT_DIR%\build-current"
set "DIST_DIR=%ARTIFACT_DIR%\dist-current"
set "SPEC_DIR=%ARTIFACT_DIR%"

if not exist "%SOURCE_DIR%\main.py" (
    echo Could not find repo-activity\main.py next to this script.
    pause
    exit /b 1
)

echo Stopping running RepoActivity.exe...
taskkill /im RepoActivity.exe /f >nul 2>nul

echo Cleaning old RepoActivity build output...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
if exist "%SPEC_DIR%\RepoActivity.spec" del /q "%SPEC_DIR%\RepoActivity.spec"

echo.
echo Building RepoActivity.exe...
where py >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    py -3 -m PyInstaller --noconfirm --clean --onedir --windowed --name RepoActivity --distpath "%DIST_DIR%" --workpath "%BUILD_DIR%" --specpath "%SPEC_DIR%" "%SOURCE_DIR%\main.py"
) else (
    python -m PyInstaller --noconfirm --clean --onedir --windowed --name RepoActivity --distpath "%DIST_DIR%" --workpath "%BUILD_DIR%" --specpath "%SPEC_DIR%" "%SOURCE_DIR%\main.py"
)

echo.
if exist "%DIST_DIR%\RepoActivity\RepoActivity.exe" (
    echo Rebuild completed: "%DIST_DIR%\RepoActivity\RepoActivity.exe"
) else (
    echo Rebuild failed.
)

pause
