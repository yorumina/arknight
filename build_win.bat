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

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

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
    echo Build failed!
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

echo.
echo === Build successful! ===
echo Run: .\build\Arknight.exe
