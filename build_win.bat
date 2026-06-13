@echo off
setlocal enabledelayedexpansion

echo.
echo === Synchronizing files to C:\ArkBuild ===
:: Exclude build, .git, temp_test, and Xcode macOS symlink paths to avoid access denied errors
robocopy "%~dp0." C:\ArkBuild /E /XD build test_build .git temp_test /XJD /XJF /R:1 /W:1
if %ERRORLEVEL% GEQ 8 (
    echo File synchronization failed with error code %ERRORLEVEL%
    exit /b 1
)

set "VCVARS="
for %%P in (
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
) do (
    if not defined VCVARS if exist "%%~P" set "VCVARS=%%~P"
)

if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%I\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

if not defined VCVARS (
    echo Visual Studio 2022 C++ build tools were not found.
    echo Install "Desktop development with C++", then run this script again.
    exit /b 1
)

call "%VCVARS%" x64
if %ERRORLEVEL% NEQ 0 (
    echo Failed to initialize Visual Studio build environment.
    exit /b 1
)

echo.
echo === Configuring CMake ===
cmake -S C:\ArkBuild -B C:\ArkBuild\build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed with error code %ERRORLEVEL%
    exit /b 1
)

echo.
echo === Building Arknight ===
cmake --build C:\ArkBuild\build --target Arknight --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo Arknight build failed!
    exit /b 1
)

echo.
echo === Building ArknightPreload ===
cmake --build C:\ArkBuild\build --target ArknightPreload --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo ArknightPreload build failed!
    exit /b 1
)

echo.
echo === Copying built executable back to workspace ===
if not exist "%~dp0build" mkdir "%~dp0build"
copy /Y C:\ArkBuild\build\Debug\Arknight.exe "%~dp0build\Arknight.exe"
if %ERRORLEVEL% NEQ 0 (
    echo Failed to copy Arknight.exe to build directory
    exit /b 1
)
copy /Y C:\ArkBuild\build\Debug\ArknightPreload.exe "%~dp0build\ArknightPreload.exe"
if %ERRORLEVEL% NEQ 0 (
    echo Failed to copy ArknightPreload.exe to build directory
    exit /b 1
)

echo.
echo === Build successful! ===
echo Preload: .\run_preload.bat
echo Run game: .\build\Arknight.exe
