@echo off
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
    py -3 serve.py %*
    goto done
)
where python >nul 2>nul
if not errorlevel 1 (
    python serve.py %*
    goto done
)
echo Python 3 is required. Install Python 3, then run this file again.
:done
pause
