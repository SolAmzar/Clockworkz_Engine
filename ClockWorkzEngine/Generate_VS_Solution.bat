@echo off
setlocal

echo =======================================================
echo Generating Visual Studio 2022 Solution for ClockWorkz
echo =======================================================

:: Go to project root (where CMakeLists.txt lives)
cd /d "%~dp0"

:: Create build folder if it doesn't exist
if not exist build (
    mkdir build
)

cd build

:: Generate the Visual Studio 2022 solution using CMake
cmake .. -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake failed to generate the project files.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo =======================================================
echo Solution generated successfully in the "build" folder!
echo Opening Visual Studio...
echo =======================================================

:: Open the generated solution in Visual Studio
if exist ClockWorkzProject.sln (
    start ClockWorkzProject.sln
) else (
    echo [ERROR] Could not find ClockWorkzProject.sln
)

endlocal
pause
