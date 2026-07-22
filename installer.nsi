; ============================================================================
; OCP - Object Creation Project  |  NSIS Installer Script
; Publisher: KitariosWebStudio - KWS
; Version:   2.2
; ============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "WinMessages.nsh"
!include "LogicLib.nsh"

; --- General ---
Name "OCP - Object Creation Project"
OutFile "OCP-v2.2-Installer.exe"
InstallDir "$PROGRAMFILES\OCP"
InstallDirRegKey HKLM "Software\OCP" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Unicode True

; --- Version Information ---
VIProductVersion "2.2.0.0"
VIAddVersionKey "ProductName" "OCP - Object Creation Project"
VIAddVersionKey "CompanyName" "KitariosWebStudio - KWS"
VIAddVersionKey "LegalCopyright" "Copyright 2026 KitariosWebStudio"
VIAddVersionKey "FileDescription" "OCP Installer"
VIAddVersionKey "FileVersion" "2.2.0.0"
VIAddVersionKey "ProductVersion" "2.2.0.0"

; --- MUI Settings ---
!define MUI_ABORTWARNING
!define MUI_ICON "E:\Object Creation Project - KWS\ocp_icon.ico"
!define MUI_UNICON "E:\Object Creation Project - KWS\ocp_icon.ico"
!define MUI_WELCOMEPAGE_TITLE "Bienvenue dans l'installeur OCP"
!define MUI_WELCOMEPAGE_TEXT "Ce wizard va installer OCP - Object Creation Project v2.2 sur votre ordinateur.$\r$\n$\r$\nOCP est un outil de creation d'objets 3D puissant et intuitif, developpe par KitariosWebStudio.$\r$\n$\r$\nCliquez sur Suivant pour continuer."
!define MUI_UNWELCOMEPAGE_TITLE "Desinstallation d'OCP"
!define MUI_UNWELCOMEPAGE_TEXT "Ce wizard va desinstaller OCP - Object Creation Project de votre ordinateur.$\r$\n$\r$\nCliquez sur Suivant pour continuer."

; --- Installer Pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "E:\Object Creation Project - KWS\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; --- Uninstaller Pages ---
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; --- Languages (French first, then English) ---
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "English"

LangString DESC_MainApp ${LANG_FRENCH} "Fichiers principaux de l'application (requis)"
LangString DESC_MainApp ${LANG_ENGLISH} "Main application files (required)"

LangString DESC_DesktopShortcut ${LANG_FRENCH} "Raccourci Bureau"
LangString DESC_DesktopShortcut ${LANG_ENGLISH} "Desktop shortcut"

LangString DESC_StartMenu ${LANG_FRENCH} "Dossier du Menu Demarrer"
LangString DESC_StartMenu ${LANG_ENGLISH} "Start Menu folder"

LangString DESC_FileAssoc ${LANG_FRENCH} "Associer les fichiers .ocp a OCP"
LangString DESC_FileAssoc ${LANG_ENGLISH} "Associate .ocp files with OCP"

; --- Installer Sections ---
Section $(DESC_MainApp) SecMainApp
    SectionIn RO

    SetOutPath "$INSTDIR"

    File "E:\Object Creation Project - KWS\cpp\OCP.exe"
    File "E:\Object Creation Project - KWS\cpp\libwinpthread-1.dll"

    WriteRegStr HKLM "Software\OCP" "InstallDir" "$INSTDIR"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "DisplayName" "OCP - Object Creation Project"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "DisplayIcon" '"$INSTDIR\OCP.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "Publisher" "KitariosWebStudio - KWS"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "DisplayVersion" "2.2"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "VersionMajor" "2"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "VersionMinor" "2"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "NoRepair" 1
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "InstallLocation" "$INSTDIR"

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP" \
        "EstimatedSize" "$0"

    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section $(DESC_DesktopShortcut) SecDesktopShortcut
    CreateShortcut "$DESKTOP\OCP.lnk" "$INSTDIR\OCP.exe" "" "$INSTDIR\OCP.exe" 0
SectionEnd

Section $(DESC_StartMenu) SecStartMenu
    CreateDirectory "$SMPROGRAMS\OCP"
    CreateShortcut "$SMPROGRAMS\OCP\OCP.lnk" "$INSTDIR\OCP.exe" "" "$INSTDIR\OCP.exe" 0
    CreateShortcut "$SMPROGRAMS\OCP\Uninstall OCP.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section $(DESC_FileAssoc) SecFileAssoc
    WriteRegStr HKCR ".ocp" "" "OCP.File"
    WriteRegStr HKCR ".ocp" "Content Type" "application/x-ocp"
    WriteRegStr HKCR "OCP.File" "" "OCP Object File"
    WriteRegStr HKCR "OCP.File\DefaultIcon" "" "$INSTDIR\OCP.exe,0"
    WriteRegStr HKCR "OCP.File\shell\open\command" "" '"$INSTDIR\OCP.exe" "%1"'
    System::Call 'Shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

; --- Component Descriptions ---
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMainApp} $(DESC_MainApp)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktopShortcut} $(DESC_DesktopShortcut)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} $(DESC_StartMenu)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecFileAssoc} $(DESC_FileAssoc)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- Uninstaller Section ---
Section "Uninstall"
    Delete "$INSTDIR\OCP.exe"
    Delete "$INSTDIR\libwinpthread-1.dll"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$DESKTOP\OCP.lnk"

    Delete "$SMPROGRAMS\OCP\OCP.lnk"
    Delete "$SMPROGRAMS\OCP\Uninstall OCP.lnk"
    RMDir "$SMPROGRAMS\OCP"

    DeleteRegKey HKCR ".ocp"
    DeleteRegKey HKCR "OCP.File"
    System::Call 'Shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP"
    DeleteRegKey HKLM "Software\OCP"
SectionEnd
