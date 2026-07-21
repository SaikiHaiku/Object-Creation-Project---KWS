@echo off
echo ============================================
echo   BloxBot Installer
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
echo Set oWS = WScript.CreateObject^("WScript.Shell"^) > "%temp%\BloxBot.vbs"
echo sLinkFile = oWS.SpecialFolders^("Desktop"^) ^& "\BloxBot.lnk" >> "%temp%\BloxBot.vbs"
echo Set oLink = oWS.CreateShortcut^(sLinkFile^) >> "%temp%\BloxBot.vbs"
echo oLink.TargetPath = "%~dp0main.py" >> "%temp%\BloxBot.vbs"
echo oLink.WorkingDirectory = "%~dp0" >> "%temp%\BloxBot.vbs"
echo oLink.Description = "BloxBot - 3D/2D Creator" >> "%temp%\BloxBot.vbs"
echo oLink.Save >> "%temp%\BloxBot.vbs"
cscript /nologo "%temp%\BloxBot.vbs"
del "%temp%\BloxBot.vbs"
echo Desktop shortcut created!

echo.
echo [4/4] Installation complete!
echo.
echo ============================================
echo   BloxBot is ready to use!
echo ============================================
echo.
echo You can now:
echo   - Double-click "BloxBot" on your desktop
echo   - Run: python main.py
echo   - Open website/index.html for documentation
echo.
pause
