@echo off
setlocal
cd /d "%~dp0"
title Mini Stronghold - English Edition
chcp 65001 >nul
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
echo ==========================================
echo       MINI STRONGHOLD - ENGLISH BUILD
echo ==========================================
echo.
if not exist "stronghold_game_english.cpp" (
    echo ERROR: stronghold_game_english.cpp was not found.
    pause
    exit /b 1
)
if not exist "C:\msys64\ucrt64\bin\g++.exe" (
    echo ERROR: MSYS2 UCRT64 GCC was not found.
    pause
    exit /b 1
)
if not exist "C:\msys64\ucrt64\include\raylib.h" (
    echo ERROR: Raylib header was not found.
    pause
    exit /b 1
)
echo Compiling game...
g++ "stronghold_game_english.cpp" "MainMenu.cpp" -o "StrongholdGame.exe" -std=c++17 -O2 -I"C:\msys64\ucrt64\include" -L"C:\msys64\ucrt64\lib" -lraylib -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 > "build.log" 2>&1
if errorlevel 1 (
    echo.
    echo BUILD FAILED. Details:
    type "build.log"
    echo.
    pause
    exit /b 1
)
echo BUILD SUCCESSFUL!
echo Starting game...
start "" "StrongholdGame.exe"
exit /b 0
