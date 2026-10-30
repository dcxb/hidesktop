@echo off
setlocal enabledelayedexpansion

rem Builds build\hidesktop.exe with the MinGW-w64 toolchain on PATH.
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "SRC=%ROOT%\src"
set "OBJ=%ROOT%\build\obj"
set "OUT=%ROOT%\build\hidesktop.exe"

if not exist "%OBJ%" mkdir "%OBJ%"

set "CXXFLAGS=-std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -municode -Wall -Wextra -Wno-unused-parameter -finput-charset=UTF-8 -fexec-charset=UTF-8"
set "LIBS=-lcomctl32 -lshell32 -luser32 -lgdi32 -ladvapi32 -lole32"

echo [1/2] resources
windres -I "%SRC%\res" "%SRC%\res\hidesktop.rc" -O coff -o "%OBJ%\hidesktop.res"
if errorlevel 1 goto :error

echo [2/2] sources
del /q "%OBJ%\*.o" 2>nul
for %%f in ("%SRC%\*.cpp") do (
    echo   %%~nxf
    g++ %CXXFLAGS% -c "%%f" -o "%OBJ%\%%~nf.o"
    if errorlevel 1 goto :error
)

echo [link]
break > "%OBJ%\link.rsp"
for %%f in ("%OBJ%\*.o") do >> "%OBJ%\link.rsp" echo "%%f"
>> "%OBJ%\link.rsp" echo "%OBJ%\hidesktop.res"

g++ -municode -mwindows -static -static-libgcc -static-libstdc++ -s -o "%OUT%" @"%OBJ%\link.rsp" %LIBS%
if errorlevel 1 goto :error

echo.
echo Built: %OUT%
exit /b 0

:error
echo.
echo Build failed.
exit /b 1
