@echo off
echo ============================================
echo   OCP Installer - KitariosWebStudio - KWS
echo ============================================
echo.

echo [1/4] Checking Python...
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found!
    echo Please install Python 3.10+ from https://python.org
    echo Make sure to check "Add Python to PATH"
    pause
    exit /b 1
)
echo Python found!

echo.
echo [2/4] Installing dependencies...
pip install -r requirements.txt
if errorlevel 1 (
    echo ERROR: Failed to install dependencies!
    pause
    exit /b 1
)
echo Dependencies installed!

echo.
echo [3/4] Creating desktop shortcut...
echo Set oWS = WScript.CreateObject^("WScript.Shell"^) > "%temp%\OCP.vbs"
echo sLinkFile = oWS.SpecialFolders^("Desktop"^) ^& "\OCP.lnk" >> "%temp%\OCP.vbs"
echo Set oLink = oWS.CreateShortcut^(sLinkFile^) >> "%temp%\OCP.vbs"
echo oLink.TargetPath = "%~dp0main.py" >> "%temp%\OCP.vbs"
echo oLink.WorkingDirectory = "%~dp0" >> "%temp%\OCP.vbs"
echo oLink.Description = "OCP - Object Creation Project" >> "%temp%\OCP.vbs"
echo oLink.Save >> "%temp%\OCP.vbs"
cscript /nologo "%temp%\OCP.vbs"
del "%temp%\OCP.vbs"
echo Desktop shortcut created!

echo.
echo [4/4] Installation complete!
echo.
echo ============================================
echo   OCP is ready to use!
echo   Developed by KitariosWebStudio - KWS
echo ============================================
echo.
echo You can now:
echo   - Double-click "OCP" on your desktop
echo   - Run: python main.py
echo   - Open website/index.html for documentation
echo.
pause
