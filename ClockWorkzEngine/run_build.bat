@echo off
setlocal

:: Go to project root (where CMakeLists.txt lives)
cd /d %~dp0

:: Create build folder if it doesn't exist
if not exist build (
    mkdir build
)

cd build

:: Configure project (generate VS2022 solution)
cmake .. -G "Visual Studio 17 2022" -A x64

:: Build in Debug mode
cmake --build . --config Debug

endlocal
pause
