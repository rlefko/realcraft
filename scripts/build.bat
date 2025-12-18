@echo off
setlocal enabledelayedexpansion

REM RealCraft Build Script for Windows
REM Usage: build.bat [debug|release] [/clean] [/test]

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..

REM Defaults
set BUILD_TYPE=debug
set CLEAN_BUILD=0
set RUN_TESTS=0

REM Parse arguments
:parse_args
if "%~1"=="" goto :end_parse_args
if /i "%~1"=="debug" (
    set BUILD_TYPE=debug
    shift
    goto :parse_args
)
if /i "%~1"=="release" (
    set BUILD_TYPE=release
    shift
    goto :parse_args
)
if /i "%~1"=="/clean" (
    set CLEAN_BUILD=1
    shift
    goto :parse_args
)
if /i "%~1"=="/test" (
    set RUN_TESTS=1
    shift
    goto :parse_args
)
if /i "%~1"=="/?" (
    echo Usage: %~nx0 [debug^|release] [/clean] [/test]
    echo.
    echo Options:
    echo   debug^|release  Build configuration (default: debug)
    echo   /clean         Remove build directory before building
    echo   /test          Run tests after building
    echo.
    exit /b 0
)
echo Unknown option: %~1
exit /b 1

:end_parse_args

set PRESET=windows-%BUILD_TYPE%
set BUILD_DIR=%PROJECT_ROOT%\build\%PRESET%

echo ==========================================
echo RealCraft Build
echo Platform:      Windows
echo Configuration: %BUILD_TYPE%
echo Preset:        %PRESET%
echo ==========================================

REM Check for vcpkg
if "%VCPKG_ROOT%"=="" (
    if exist "%PROJECT_ROOT%\vcpkg" (
        set VCPKG_ROOT=%PROJECT_ROOT%\vcpkg
        echo Using local vcpkg: !VCPKG_ROOT!
    ) else (
        echo Error: VCPKG_ROOT not set and vcpkg not found in project root
        echo Run: scripts\setup-vcpkg.bat
        exit /b 1
    )
)

REM Clean if requested
if %CLEAN_BUILD%==1 (
    if exist "%BUILD_DIR%" (
        echo Cleaning build directory...
        rmdir /s /q "%BUILD_DIR%"
    )
)

REM Configure
echo.
echo Configuring...
cmake --preset %PRESET%
if errorlevel 1 exit /b 1

REM Build
echo.
echo Building...
cmake --build --preset %PRESET%
if errorlevel 1 exit /b 1

REM Test if requested
if %RUN_TESTS%==1 (
    echo.
    echo Running tests...
    ctest --preset %PRESET%
)

echo.
echo ==========================================
echo Build complete!
echo Output: %BUILD_DIR%
echo ==========================================

exit /b 0
