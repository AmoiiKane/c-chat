@echo off
setlocal enabledelayedexpansion
title c-chat Setup
color 0A

echo.
echo  =============================================
echo   c-chat Setup
echo  =============================================
echo.

:: ── Check if running as Administrator ─────────────────────────
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo  [!] This script needs Administrator privileges.
    echo      Right-click setup.bat and choose "Run as administrator".
    echo.
    pause
    exit /b 1
)

:: ── Check internet connection ──────────────────────────────────
ping -n 1 8.8.8.8 >nul 2>&1
if %errorlevel% neq 0 (
    echo  [!] No internet connection detected.
    echo      Please connect to the internet and try again.
    echo.
    pause
    exit /b 1
)

:: ── Fix project structure ──────────────────────────────────────
echo  [1/5] Checking project structure...

if not exist src mkdir src

:: Move source files to src\ if they ended up in root
if exist client.c    move /Y client.c    src\ >nul
if exist server.c    move /Y server.c    src\ >nul
if exist common.h    move /Y common.h    src\ >nul
if exist gui_client.c move /Y gui_client.c src\ >nul

if not exist src\server.c (
    echo  [!] src\server.c not found. Make sure you cloned the full repo.
    pause
    exit /b 1
)
echo      OK

:: ── Check / install winget ────────────────────────────────────
echo  [2/5] Checking winget...
winget --version >nul 2>&1
if %errorlevel% neq 0 (
    echo  [!] winget not found. Please update Windows or install
    echo      App Installer from the Microsoft Store, then re-run.
    start https://apps.microsoft.com/store/detail/app-installer/9NBLGGH4NNS1
    pause
    exit /b 1
)
echo      OK

:: ── Check / install MinGW (gcc + make) ────────────────────────
echo  [3/5] Checking GCC / MinGW...
where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo      GCC not found. Installing MinGW-w64 via winget...
    winget install -e --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements
    if %errorlevel% neq 0 (
        echo  [!] MinGW install failed. Install manually from https://winlibs.com
        pause
        exit /b 1
    )
    :: Add MSYS2 mingw64 bin to PATH for this session
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
    :: Persist to user PATH
    setx PATH "C:\msys64\mingw64\bin;%PATH%" >nul
    echo      MinGW installed. PATH updated.
) else (
    echo      OK
)

:: ── Check / install make ──────────────────────────────────────
echo  [4/5] Checking make...
where make >nul 2>&1
if %errorlevel% neq 0 (
    where mingw32-make >nul 2>&1
    if %errorlevel% equ 0 (
        :: mingw32-make exists — create a make.bat shim in the same folder
        for /f "tokens=*" %%i in ('where mingw32-make') do set MINGW_MAKE=%%i
        for %%i in ("!MINGW_MAKE!") do set MINGW_BIN=%%~dpi
        echo @echo off > "!MINGW_BIN!make.bat"
        echo mingw32-make %%* >> "!MINGW_BIN!make.bat"
        echo      Created make.bat shim at !MINGW_BIN!
    ) else (
        echo      make not found. Installing GnuWin32 make via winget...
        winget install -e --id GnuWin32.Make --accept-source-agreements --accept-package-agreements
        if %errorlevel% neq 0 (
            echo  [!] make install failed. Install manually from https://gnuwin32.sourceforge.net/packages/make.htm
            pause
            exit /b 1
        )
        set "PATH=C:\Program Files (x86)\GnuWin32\bin;%PATH%"
        setx PATH "C:\Program Files (x86)\GnuWin32\bin;%PATH%" >nul
        echo      make installed. PATH updated.
    )
) else (
    echo      OK
)

:: ── Build ─────────────────────────────────────────────────────
echo  [5/5] Building c-chat...
if not exist bin mkdir bin

make 2>&1
if %errorlevel% neq 0 (
    echo.
    echo  [!] Build failed. Check the errors above.
    echo      Make sure gcc is installed and in PATH.
    pause
    exit /b 1
)

:: ── Done ──────────────────────────────────────────────────────
echo.
echo  =============================================
echo   Build complete!
echo  =============================================
echo.
echo   Binaries in bin\:
if exist bin\server.exe     echo     bin\server.exe      — run this on the host machine
if exist bin\client.exe     echo     bin\client.exe      — terminal client
if exist bin\gui_client.exe echo     bin\gui_client.exe  — graphical client (no terminal needed)
echo.
echo   Quick start:
echo     1. On the server machine:   bin\server.exe
echo     2. On client machines:      bin\gui_client.exe
echo                              or bin\client.exe ^<server-ip^>
echo.
pause
