@echo off
rem ---------------------------------------------------------------
rem  build_Purepynet_exe.bat  --  turns Purepynet.py into Purepynet.exe
rem  Uses Python 3.12 64-bit through the "py" launcher (py -3.12),
rem  so the Windows Store "python" shortcut is never touched.
rem  Put this .bat next to Purepynet.py and double-click it.
rem ---------------------------------------------------------------
cd /d "%~dp0"
set "PY=py -3.12"

if not exist "Purepynet.py" (
    echo ERROR: Purepynet.py not found in %~dp0
    goto end
)

echo Python used:
%PY% -c "import sys,struct; print(' ', sys.version.split()[0], struct.calcsize('P')*8, 'bit', sys.executable)"
if errorlevel 1 (
    echo ERROR: Python 3.12 not found. Installed versions:
    py -0p
    goto end
)

echo.
echo Installing / updating PyInstaller ...
%PY% -m pip install --upgrade --disable-pip-version-check pyinstaller
if errorlevel 1 (
    echo ERROR: could not install PyInstaller.
    goto end
)

echo.
echo Building Purepynet.exe ...
%PY% -m PyInstaller --onefile --console --clean --noconfirm ^
    --name Purepynet --distpath . --workpath build_tmp --specpath build_tmp ^
    Purepynet.py
if errorlevel 1 (
    echo.
    echo BUILD FAILED - see the errors above.
    goto end
)

rmdir /s /q build_tmp 2>nul
echo.
echo OK: Purepynet.exe built in %~dp0

:end
echo.
pause
