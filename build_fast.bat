@echo off
set MSBUILD="C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe"
set PARAMS=/m /nr:false /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TrackFileAccess=false

echo Building common...
%MSBUILD% build\src\common\common.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild common failed!
    exit /b %ERRORLEVEL%
)

echo Building audio_core...
%MSBUILD% build\src\audio_core\audio_core.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild audio_core failed!
    exit /b %ERRORLEVEL%
)

echo Building video_core...
%MSBUILD% build\src\video_core\video_core.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild video_core failed!
    exit /b %ERRORLEVEL%
)

echo Building core...
%MSBUILD% build\src\core\core.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild core failed!
    exit /b %ERRORLEVEL%
)

echo Building input_common...
%MSBUILD% build\src\input_common\input_common.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild input_common failed!
    exit /b %ERRORLEVEL%
)

echo Building qt_common...
%MSBUILD% build\src\qt_common\qt_common.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild qt_common failed!
    exit /b %ERRORLEVEL%
)

echo Building frontend_common...
%MSBUILD% build\src\frontend_common\frontend_common.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild frontend_common failed!
    exit /b %ERRORLEVEL%
)

echo Building eden...
%MSBUILD% build\src\eden\eden.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild eden failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Compilation completed successfully.


%MSBUILD% build\src\eden_cmd\eden-cmd.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild eden_cmd failed!
    exit /b %ERRORLEVEL%
)

echo Building dedicated_room...
%MSBUILD% build\src\dedicated_room\eden-room.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild dedicated_room failed!
    exit /b %ERRORLEVEL%
)

echo Building eden_room_standalone...
%MSBUILD% build\src\eden_room_standalone\eden_room_standalone.vcxproj %PARAMS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild eden_room_standalone failed!
    exit /b %ERRORLEVEL%
)

pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign_binaries.ps1"
if %ERRORLEVEL% neq 0 (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign_binaries.ps1"
)
