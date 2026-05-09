@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

:: Resolve script directory (handles spaces and Unicode in path)
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

:: Set up paths relative to script location
set "TRX_KERNEL_DIR=%SCRIPT_DIR%\..\share\trx_vanity\kernel"
set "TRX_BIN=%SCRIPT_DIR%\trx_vanity.exe"
set "DOCS_DIR=%SCRIPT_DIR%\..\share\trx_vanity\docs"

:: Verify binary exists
if not exist "%TRX_BIN%" (
    echo [ERROR] trx_vanity.exe not found at: %TRX_BIN%
    echo.
    echo Please ensure you extracted the ZIP package correctly,
    echo or reinstall using the NSIS installer.
    pause
    exit /b 1
)

:: Check if TRX_KERNEL_DIR exists (for GPU mode)
if not exist "%TRX_KERNEL_DIR%\vanity.cl" (
    echo [WARNING] OpenCL kernel not found. GPU mode will not be available.
    echo           Path checked: %TRX_KERNEL_DIR%
    echo.
    echo CPU mode will work normally.
    echo.
)

:: Print welcome banner
cls
echo ╔══════════════════════════════════════════════════════════════╗
echo ║           TRX Vanity Address Generator  (Windows)              ║
echo ║                    CLI Alpha Version                           ║
echo ╠══════════════════════════════════════════════════════════════╣
echo ║  Security Notice: Private keys are hidden by default.          ║
echo ║  Only use --show-private-key if you understand the risk.       ║
echo ║  Never share private keys, result files, or screenshots.     ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

:: Show help
echo [INFO] Showing CLI help...
echo.
"%TRX_BIN%" --help
echo.

:: Print usage examples
echo ──────────────────────────────────────────────────────────────
echo Quick Start Examples:
echo ──────────────────────────────────────────────────────────────
echo.
echo 1. CPU smoke test (verify installation):
echo    trx_vanity.exe prefix T --max-attempts 64 -t 1
echo.
echo 2. Simple suffix search:
echo    trx_vanity.exe suffix 8888 -t 8 -v
echo.
echo 3. GPU mode (if kernel available):
echo    set TRX_KERNEL_DIR=%TRX_KERNEL_DIR%
echo    trx_vanity.exe suffix 8888 --gpu --batch-size 65536 -v
echo.
echo 4. Save results (private key hidden by default):
echo    trx_vanity.exe suffix 8888 -o results.csv
echo.

:: Show docs paths
if exist "%DOCS_DIR%\USER_GUIDE_zh.md" (
    echo Documentation: %DOCS_DIR%\USER_GUIDE_zh.md
)
if exist "%DOCS_DIR%\SECURITY_zh.md" (
    echo Security Guide:  %DOCS_DIR%\SECURITY_zh.md
)
if exist "%DOCS_DIR%\WINDOWS_INSTALL_zh.md" (
    echo Windows Guide:   %DOCS_DIR%\WINDOWS_INSTALL_zh.md
)
echo.
echo [TIP] Type commands above and press Enter. Press Ctrl+C to stop.
echo [TIP] Close this window when finished.
echo.

:: Keep window open for interactive use
cmd /k
