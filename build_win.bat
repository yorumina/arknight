@echo off
setlocal

set "WORKSPACE=%~dp0"
set "STAGING=C:\ArkBuild"
set "BUILD_DIR=%WORKSPACE%build"
set "LOG_DIR=%BUILD_DIR%\logs"
set "LOG_DIR_TEXT=build\logs"
set "SYNC_LOG=%LOG_DIR%\build_win_sync.log"
set "ENV_LOG=%LOG_DIR%\build_win_env.log"
set "CONFIG_LOG=%LOG_DIR%\build_win_configure.log"
set "BUILD_LOG=%LOG_DIR%\build_win_build.log"
set "COPY_LOG=%LOG_DIR%\build_win_copy.log"
set "VSLANG=1033"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" >nul 2>&1
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1

echo.
echo === Arknight Windows Build ===
echo Logs: %LOG_DIR_TEXT%
echo.
echo [1/5] Syncing source to C:\ArkBuild...
robocopy "%WORKSPACE%." "%STAGING%" /E /XD build test_build .git temp_test data docs /XF *.md *.obj .gitignore build_log.txt cmake_output.txt extract.sh imgui.ini /XJD /XJF /R:1 /W:1 /NFL /NDL /NJH /NJS /NP > "%SYNC_LOG%" 2>&1
set "ROBOCOPY_EXIT=%ERRORLEVEL%"
if %ROBOCOPY_EXIT% GEQ 8 (
    echo [ERROR] File sync failed. See %LOG_DIR_TEXT%\build_win_sync.log.
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

echo [2/5] Preparing Visual Studio build environment...
call "%VCVARS%" x64 > "%ENV_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to initialize Visual Studio build environment. See %LOG_DIR_TEXT%\build_win_env.log.
    exit /b 1
)

echo.
echo [3/5] Configuring CMake...
cmake -S "%STAGING%" -B "%STAGING%\build" -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug -DOpenGL_GL_PREFERENCE=LEGACY -Wno-deprecated > "%CONFIG_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed. See %LOG_DIR_TEXT%\build_win_configure.log.
    exit /b 1
)

echo.
echo [4/5] Building executables...
cmake --build "%STAGING%\build" --target Arknight --config Debug -- /nologo /v:quiet /clp:ErrorsOnly;NoSummary /p:PreferredUILang=en-US > "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Arknight build failed. See %LOG_DIR_TEXT%\build_win_build.log.
    exit /b 1
)
cmake --build "%STAGING%\build" --target ArknightPreload --config Debug -- /nologo /v:quiet /clp:ErrorsOnly;NoSummary /p:PreferredUILang=en-US >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] ArknightPreload build failed. See %LOG_DIR_TEXT%\build_win_build.log.
    exit /b 1
)
cmake --build "%STAGING%\build" --target ArknightBuilder --config Debug -- /nologo /v:quiet /clp:ErrorsOnly;NoSummary /p:PreferredUILang=en-US >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] ArknightBuilder build failed. See %LOG_DIR_TEXT%\build_win_build.log.
    exit /b 1
)

echo.
echo [5/5] Copying outputs...
copy /Y "%STAGING%\build\Debug\Arknight.exe" "%BUILD_DIR%\Arknight.exe" > "%COPY_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to copy Arknight.exe. See %LOG_DIR_TEXT%\build_win_copy.log.
    exit /b 1
)
copy /Y "%STAGING%\build\Debug\ArknightPreload.exe" "%BUILD_DIR%\ArknightPreload.exe" >> "%COPY_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to copy ArknightPreload.exe. See %LOG_DIR_TEXT%\build_win_copy.log.
    exit /b 1
)
copy /Y "%STAGING%\build\Debug\ArknightBuilder.exe" "%BUILD_DIR%\ArknightBuilder.exe" >> "%COPY_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to copy ArknightBuilder.exe. See %LOG_DIR_TEXT%\build_win_copy.log.
    exit /b 1
)

echo.
echo === Build successful ===
echo Preload: .\run_preload.bat
echo Run game: .\build\Arknight.exe
echo Builder: .\build\ArknightBuilder.exe
