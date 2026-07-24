@echo off
title OCP Installer - KitariosWebStudio - KWS
echo.
echo  ============================================
echo   OCP - Object Creation Project
echo   KitariosWebStudio - KWS
echo  ============================================
echo.
echo  Installing OCP...
echo.

set "INSTALL_DIR=%LOCALAPPDATA%\OCP"
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

echo  Copying files to %INSTALL_DIR% ...
copy /Y "%~dp0OCP.exe" "%INSTALL_DIR%\OCP.exe" >nul 2>&1
copy /Y "%~dp0libwinpthread-1.dll" "%INSTALL_DIR%\libwinpthread-1.dll" >nul 2>&1

echo  Creating desktop shortcut...
powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%USERPROFILE%\Desktop\OCP.lnk'); $s.TargetPath='%INSTALL_DIR%\OCP.exe'; $s.WorkingDirectory='%INSTALL_DIR%'; $s.Description='OCP - Object Creation Project'; $s.Save()"

echo.
echo  ============================================
echo   Installation complete!
echo   OCP has been installed to:
echo   %INSTALL_DIR%
echo.
echo   A shortcut has been placed on your Desktop.
echo  ============================================
echo.

echo  Launching OCP...
start "" "%INSTALL_DIR%\OCP.exe"
