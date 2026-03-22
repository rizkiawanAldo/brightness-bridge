@echo off
REM Build brightness_bridge.exe
REM Uses w64devkit gcc from C:\tmp\w64devkit\bin
REM You can also use any MinGW gcc on PATH.

set "PATH=C:\tmp\w64devkit\bin;%PATH%"

echo Compiling resources...
windres resource.rc -o resource.o

echo Building brightness_bridge.exe...
gcc -O2 -mwindows -o brightness_bridge.exe brightness_bridge.c resource.o

if %ERRORLEVEL% EQU 0 (
    echo.
    echo   SUCCESS: brightness_bridge.exe created
    for %%A in (brightness_bridge.exe) do echo   Size: %%~zA bytes
    echo.
) else (
    echo.
    echo   BUILD FAILED - make sure gcc is available
    echo.
)
