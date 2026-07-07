@echo off
"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" build\src\qt_common\qt_common.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TrackFileAccess=false
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild qt_common failed!
    exit /b %ERRORLEVEL%
)
"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" build\src\frontend_common\frontend_common.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TrackFileAccess=false
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild frontend_common failed!
    exit /b %ERRORLEVEL%
)
"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" build\src\core\core.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TrackFileAccess=false
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild core failed!
    exit /b %ERRORLEVEL%
)
"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" build\src\eden\eden.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TrackFileAccess=false
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild eden failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Compilation completed successfully.

"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" build\src\eden_cmd\eden-cmd.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:TrackFileAccess=false
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild eden_cmd failed!
    exit /b %ERRORLEVEL%
)
