@echo off
setlocal enabledelayedexpansion

REM Root of your project (this .bat should sit in the project root)
set ROOT=%~dp0

echo Files containing [Lebon] with line counts:
echo -----------------------------------------

set total=0

for /r "%ROOT%src" %%F in (*.cpp *.hpp *.h) do (
    findstr /c:"[Lebon]" "%%F" >nul
    if !errorlevel! == 0 (
        for /f "tokens=* delims=" %%A in ('find /c /v "" "%%F"') do (
            for %%B in (%%A) do set lines=%%B
            echo %%~nxF - !lines!
            set /a total+=!lines!
        )
    )
)

for /r "%ROOT%include" %%F in (*.cpp *.hpp *.h) do (
    findstr /c:"[Lebon]" "%%F" >nul
    if !errorlevel! == 0 (
        for /f "tokens=* delims=" %%A in ('find /c /v "" "%%F"') do (
            for %%B in (%%A) do set lines=%%B
            echo %%~nxF - !lines!
            set /a total+=!lines!
        )
    )
)

echo -----------------------------------------
echo Total lines across all [Lebon] files: %total%
echo Done.


echo Files containing [Amirul Syafiq] with line counts:
echo -----------------------------------------

set total=0

for /r "%ROOT%src" %%F in (*.cpp *.hpp *.h) do (
    findstr /c:"[Amirul Syafiq]" "%%F" >nul
    if !errorlevel! == 0 (
        for /f "tokens=* delims=" %%A in ('find /c /v "" "%%F"') do (
            for %%B in (%%A) do set lines=%%B
            echo %%~nxF - !lines!
            set /a total+=!lines!
        )
    )
)

for /r "%ROOT%include" %%F in (*.cpp *.hpp *.h) do (
    findstr /c:"[Amirul Syafiq]" "%%F" >nul
    if !errorlevel! == 0 (
        for /f "tokens=* delims=" %%A in ('find /c /v "" "%%F"') do (
            for %%B in (%%A) do set lines=%%B
            echo %%~nxF - !lines!
            set /a total+=!lines!
        )
    )
)

echo -----------------------------------------
echo Total lines across all [Amirul Syafiq] files: %total%
echo Done.
pause
