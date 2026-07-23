; ============================================================================
; OCP - Object Creation Project  |  NSIS Installer Script
; Publisher: KitariosWebStudio - KWS
; Version:   2.3
; Features:  Embeds all non-HDRI resources, auto-extracts HDRI pack if found
; ============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "WinMessages.nsh"
!include "LogicLib.nsh"
!include "TextFunc.nsh"

SetCompressor /SOLID lzma

Name "OCP - Object Creation Project"
OutFile "OCP-v2.3-Setup.exe"
InstallDir "$PROGRAMFILES\OCP-KWS"
InstallDirRegKey HKLM "Software\OCP-KWS" "InstallDir"
RequestExecutionLevel admin
Unicode True

; --- Version Information ---
VIProductVersion "2.3.0.0"
VIAddVersionKey "ProductName" "OCP - Object Creation Project"
VIAddVersionKey "CompanyName" "KitariosWebStudio - KWS"
VIAddVersionKey "LegalCopyright" "Copyright 2026 KitariosWebStudio"
VIAddVersionKey "FileDescription" "OCP v2.3 Installer"
VIAddVersionKey "FileVersion" "2.3.0.0"
VIAddVersionKey "ProductVersion" "2.3.0.0"

; --- MUI Settings ---
!define MUI_ABORTWARNING
!define MUI_ICON "E:\Object Creation Project - KWS\ocp_icon.ico"
!define MUI_UNICON "E:\Object Creation Project - KWS\ocp_icon.ico"
!define MUI_WELCOMEPAGE_TITLE "Bienvenue dans l'installeur OCP v2.3"
!define MUI_WELCOMEPAGE_TEXT "Ce wizard va installer OCP - Object Creation Project v2.3 sur votre ordinateur.$\r$\n$\r$\nOCP est un editeur 3D professionnel developpe par KitariosWebStudio - KWS.$\r$\n$\r$\nFonctionnalites :$\r$\n  - 60+ generateurs IA procedural$\r$\n  - Modeles PBR avec eclairage realiste$\r$\n  - Import/Export OBJ, STL, PNG$\r$\n  - Undo/Redo, Transformations, Groupe$\r$\n$\r$\nCliquez sur Suivant pour continuer."
!define MUI_UNWELCOMEPAGE_TITLE "Desinstallation d'OCP"
!define MUI_UNWELCOMEPAGE_TEXT "Ce wizard va desinstaller OCP de votre ordinateur."

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

; --- Languages ---
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "English"

; --- Strings ---
LangString DESC_MainApp ${LANG_FRENCH} "Application principale OCP (requis)"
LangString DESC_MainApp ${LANG_ENGLISH} "OCP main application (required)"

LangString DESC_Textures ${LANG_FRENCH} "Textures et materiaux (80+ textures PBR, 78 materiaux)"
LangString DESC_Textures ${LANG_ENGLISH} "Textures and materials (80+ PBR textures, 78 materials)"

LangString DESC_Fonts ${LANG_FRENCH} "Polices 3D (200 polices de dessin)"
LangString DESC_Fonts ${LANG_ENGLISH} "3D Fonts (200 drawing fonts)"

LangString DESC_Models ${LANG_FRENCH} "Modeles 3D (148 templates de maillages)"
LangString DESC_Models ${LANG_ENGLISH} "3D Models (148 mesh templates)"

LangString DESC_Scenes ${LANG_FRENCH} "Templates de scenes (12 scenes pre-configurees)"
LangString DESC_Scenes ${LANG_ENGLISH} "Scene templates (12 pre-configured scenes)"

LangString DESC_Brushes ${LANG_FRENCH} "Pinceaux (39 pinceaux de dessin)"
LangString DESC_Brushes ${LANG_ENGLISH} "Brushes (39 drawing brushes)"

LangString DESC_Hires ${LANG_FRENCH} "Textures Haute Resolution (44 textures 2048x2048)"
LangString DESC_Hires ${LANG_ENGLISH} "High-Res Textures (44 textures 2048x2048)"

LangString DESC_Desktop ${LANG_FRENCH} "Raccourci Bureau"
LangString DESC_Desktop ${LANG_ENGLISH} "Desktop shortcut"

LangString DESC_StartMenu ${LANG_FRENCH} "Raccourci Menu Demarrer"
LangString DESC_StartMenu ${LANG_ENGLISH} "Start Menu shortcut"

LangString DESC_FileAssoc ${LANG_FRENCH} "Associer les fichiers .ocp a OCP"
LangString DESC_FileAssoc ${LANG_ENGLISH} "Associate .ocp files with OCP"

; ============================================================
; SECTIONS
; ============================================================

Section $(DESC_MainApp) SecMainApp
    SectionIn RO

    SetOutPath "$INSTDIR"

    ; --- Core files ---
    File "E:\Object Creation Project - KWS\cpp\OCP.exe"
    File "E:\Object Creation Project - KWS\cpp\libwinpthread-1.dll"

    ; --- Bundled 7-Zip CLI (for HDRI pack extraction) ---
    SetOutPath "$INSTDIR\tools"
    File "E:\Object Creation Project - KWS\installer_staging\7za.exe"
    File "E:\Object Creation Project - KWS\installer_staging\7z.dll"

    ; --- Registry ---
    WriteRegStr HKLM "Software\OCP-KWS" "InstallDir" "$INSTDIR"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "DisplayName" "OCP - Object Creation Project"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "DisplayIcon" '"$INSTDIR\OCP.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "Publisher" "KitariosWebStudio - KWS"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "DisplayVersion" "2.3"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "VersionMajor" "2"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "VersionMinor" "3"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "NoRepair" 1
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "InstallLocation" "$INSTDIR"

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS" \
        "EstimatedSize" "$0"

    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section $(DESC_Textures) SecTextures
    SetOutPath "$INSTDIR\resources\textures"
    File /r "E:\Object Creation Project - KWS\resources\textures\*.*"
    SetOutPath "$INSTDIR\resources"
    File "E:\Object Creation Project - KWS\resources\index.json"
SectionEnd

Section $(DESC_Hires) SecHires
    SetOutPath "$INSTDIR\resources\textures_hires"
    File /r "E:\Object Creation Project - KWS\resources\textures_hires\*.*"
SectionEnd

Section $(DESC_Fonts) SecFonts
    SetOutPath "$INSTDIR\resources\fonts"
    File /r "E:\Object Creation Project - KWS\resources\fonts\*.*"
SectionEnd

Section $(DESC_Models) SecModels
    SetOutPath "$INSTDIR\resources\models"
    File /r "E:\Object Creation Project - KWS\resources\models\*.*"
SectionEnd

Section $(DESC_Scenes) SecScenes
    SetOutPath "$INSTDIR\resources\scenes"
    File /r "E:\Object Creation Project - KWS\resources\scenes\*.*"

    SetOutPath "$INSTDIR\resources\materials"
    File /r "E:\Object Creation Project - KWS\resources\materials\*.*"
SectionEnd

Section $(DESC_Brushes) SecBrushes
    SetOutPath "$INSTDIR\resources\brushes"
    File /r "E:\Object Creation Project - KWS\resources\brushes\*.*"
SectionEnd

Section $(DESC_Desktop) SecDesktop
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

; --- Post-install: Auto-extract HDRI pack if found ---
Section "-PostInstall"
    ; Check if HDRI resource pack exists next to installer
    ; The user may have downloaded OCP-HDRI-Pack.7z alongside the setup.exe
    IfFileExists "$EXEDIR\OCP-HDRI-Pack.7z" 0 +3
        DetailPrint "HDRI Resource Pack found! Extracting..."
        ExecWait '"$INSTDIR\tools\7za.exe" x "$EXEDIR\OCP-HDRI-Pack.7z" -o"$INSTDIR\resources" -y'

    IfFileExists "$EXEDIR\OCP-resources.7z" 0 +3
        DetailPrint "Full resource pack found! Extracting..."
        ExecWait '"$INSTDIR\tools\7za.exe" x "$EXEDIR\OCP-resources.7z" -o"$INSTDIR\resources" -y'
SectionEnd

; --- Component Descriptions ---
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMainApp} $(DESC_MainApp)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTextures} $(DESC_Textures)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecHires} $(DESC_Hires)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecFonts} $(DESC_Fonts)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecModels} $(DESC_Models)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecScenes} $(DESC_Scenes)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecBrushes} $(DESC_Brushes)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DESC_Desktop)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} $(DESC_StartMenu)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecFileAssoc} $(DESC_FileAssoc)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- Uninstaller ---
Section "Uninstall"
    Delete "$INSTDIR\OCP.exe"
    Delete "$INSTDIR\libwinpthread-1.dll"
    RMDir /r "$INSTDIR\tools"
    RMDir /r "$INSTDIR\resources"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$DESKTOP\OCP.lnk"

    Delete "$SMPROGRAMS\OCP\OCP.lnk"
    Delete "$SMPROGRAMS\OCP\Uninstall OCP.lnk"
    RMDir "$SMPROGRAMS\OCP"

    DeleteRegKey HKCR ".ocp"
    DeleteRegKey HKCR "OCP.File"
    System::Call 'Shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OCP-KWS"
    DeleteRegKey HKLM "Software\OCP-KWS"
SectionEnd
