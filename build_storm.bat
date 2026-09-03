@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo STORM SWITCH - Automated Build and Packaging Script
echo ===================================================

call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 (
    echo [ERROR] Failed to set up Visual Studio environment.
    exit /b 1
)

set "PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;E:\STORM EDEN 3\tools\glslang\bin;C:\Users\ReiKatari\glslang\bin"

set "SRC_DIR=e:\STORM EDEN 3\src"
set "BUILD_DIR=e:\STORM EDEN 3\build"
set "OUTPUT_DIR=e:\STORM EDEN 3\Assembling"

set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo [1/3] Running CMake configuration...
"%CMAKE%" -G "Ninja" -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DGIT_TAG="7.2.9" ^
    -DGIT_RELEASE="7.2.9" ^
    -DENABLE_QT=ON ^
    -DENABLE_QT_TRANSLATION=ON ^
    -DYUZU_USE_BUNDLED_QT=ON ^
    -DYUZU_CMD=ON ^
    -DYUZU_ROOM=ON ^
    -DYUZU_ROOM_STANDALONE=ON ^
    -DENABLE_WEB_SERVICE=OFF ^
    -DBUILD_TESTING=OFF ^
    "%SRC_DIR%"

if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [2/3] Compiling STORM SWITCH with Ninja...
"%CMAKE%" --build . --config Release
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

echo [3/3] Packaging into %OUTPUT_DIR%...
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
xcopy /E /Y /I bin\* "%OUTPUT_DIR%\"

if not exist "%OUTPUT_DIR%\user" (
    mkdir "%OUTPUT_DIR%\user"
    mkdir "%OUTPUT_DIR%\user\config"
    mkdir "%OUTPUT_DIR%\user\keys"
    mkdir "%OUTPUT_DIR%\user\nand"
    mkdir "%OUTPUT_DIR%\user\load"
    mkdir "%OUTPUT_DIR%\user\screenshots"
    mkdir "%OUTPUT_DIR%\user\sdmc"
    mkdir "%OUTPUT_DIR%\user\shader"
)

if exist "L:\CONSOLES\Nintendo Switch\STORM EDEN\user\keys" (
    xcopy /Y /I "L:\CONSOLES\Nintendo Switch\STORM EDEN\user\keys\*" "%OUTPUT_DIR%\user\keys\"
)

echo ===================================================
echo STORM SWITCH build and assembly completed successfully!
echo ===================================================
