@echo off
setlocal
pushd "%~dp0"

if not exist ".\build\ArknightPreload.exe" (
    echo [ERROR] .\build\ArknightPreload.exe was not found.
    echo Please run .\build_win.bat first.
    popd
    exit /b 1
)

set ARKNIGHT_ANIMATION_DISK_CACHE=1
set ARKNIGHT_ANIMATION_TEXTURE_CACHE=0

echo Running headless Arknight animation preload...
.\build\ArknightPreload.exe
set PRELOAD_EXIT=%ERRORLEVEL%
if not "%PRELOAD_EXIT%"=="0" (
    echo.
    echo [ERROR] Preload failed with exit code %PRELOAD_EXIT%.
    popd
    exit /b %PRELOAD_EXIT%
)

echo.
echo Preload completed.
echo Run the game with: .\build\Arknight.exe
popd
exit /b 0
