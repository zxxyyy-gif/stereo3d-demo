@echo off
setlocal
echo ============================================================
echo  devkitPro one-click installer for Nintendo 3DS homebrew
echo ============================================================
echo.
echo  NOTE: this script needs access to pkg.devkitpro.org.
echo  If that domain is blocked on your network (Cloudflare 403),
echo  the install WILL fail - use a proxy/VPN, or build in the
echo  cloud via .github/workflows/build.yml instead.
echo.
pause

where curl >nul 2>&1
if errorlevel 1 (
    echo [!] curl not found. Windows 10 1803 or newer is required.
    pause
    exit /b 1
)

set "SETUP=%TEMP%\devkitProUpdater.exe"

echo [1/3] Downloading the official installer ...
curl -L --fail --max-time 300 -o "%SETUP%" "https://github.com/devkitPro/installer/releases/download/v3.0.3/devkitProUpdater-3.0.3.exe"
if errorlevel 1 (
    echo [!] Download failed.
    pause
    exit /b 1
)

echo [2/3] Running the installer. Click YES on the UAC prompt,
echo       keep the default path C:\devkitPro, and let it finish.
"%SETUP%"

echo [3/3] Installing 3ds-dev packages with pacman ...
if not exist "C:\devkitPro\msys2\usr\bin\bash.exe" (
    echo [!] C:\devkitPro\msys2 not found - the installer did not complete.
    echo     Most likely pkg.devkitpro.org was unreachable.
    pause
    exit /b 1
)

"C:\devkitPro\msys2\usr\bin\bash.exe" -lc "pacman -S --noconfirm --needed 3ds-dev"
if errorlevel 1 (
    echo [!] pacman failed.
    echo     If the error mentions 403 Forbidden, the package mirror
    echo     is blocked on this network. Use a proxy/VPN and re-run.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Done. Toolchain installed at C:\devkitPro
echo.
echo  Build the demo:
echo    C:\devkitPro\msys2\usr\bin\bash.exe -lc "cd /c/Users/11422/WorkBuddy/2026-10-19-01-27-13/stereo3d-demo && make"
echo.
echo  Output: stereo3d-demo\stereodemo.3dsx
echo ============================================================
pause
