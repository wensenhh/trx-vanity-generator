!include "MUI2.nsh"

; General
Name "TRX Vanity Generator"
OutFile "${CPACK_PACKAGE_FILE_NAME}.exe"
InstallDir "$PROGRAMFILES64\trx_vanity"
InstallDirRegKey HKCU "Software\trx_vanity" ""
RequestExecutionLevel admin

; Interface Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}\README.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "English"

; Installer Sections
Section "Install"
    SetOutPath "$INSTDIR"

    ; Install binaries
    File "${CMAKE_CURRENT_BINARY_DIR}\Release\trx_vanity.exe"
    File "${CMAKE_CURRENT_SOURCE_DIR}\packaging\windows\trx_vanity_gui.bat"

    ; Install docs
    File "${CMAKE_CURRENT_SOURCE_DIR}\README.md"
    SetOutPath "$INSTDIR\share\trx_vanity\docs"
    File "${CMAKE_CURRENT_SOURCE_DIR}\docs\USER_GUIDE_zh.md"
    File "${CMAKE_CURRENT_SOURCE_DIR}\docs\SECURITY_zh.md"
    File "${CMAKE_CURRENT_SOURCE_DIR}\docs\PACKAGING_zh.md"
    File "${CMAKE_CURRENT_SOURCE_DIR}\packaging\windows\README_zh.md"

    ; Install kernel (if OpenCL enabled)
    SetOutPath "$INSTDIR\share\trx_vanity\kernel"
    File "${CMAKE_CURRENT_SOURCE_DIR}\kernel\vanity.cl"
    File "${CMAKE_CURRENT_SOURCE_DIR}\kernel\ecc.cl"
    File "${CMAKE_CURRENT_SOURCE_DIR}\kernel\test_ecc.cl"

    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\TRX Vanity"
    CreateShortcut "$SMPROGRAMS\TRX Vanity\TRX Vanity (GUI Launcher).lnk" "$INSTDIR\trx_vanity_gui.bat" "" "$INSTDIR\trx_vanity.exe" 0
    CreateShortcut "$SMPROGRAMS\TRX Vanity\TRX Vanity CLI.lnk" "$INSTDIR\trx_vanity.exe" "" "$INSTDIR\trx_vanity.exe" 0
    CreateShortcut "$SMPROGRAMS\TRX Vanity\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    CreateShortcut "$DESKTOP\TRX Vanity.lnk" "$INSTDIR\trx_vanity_gui.bat" "" "$INSTDIR\trx_vanity.exe" 0

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Registry
    WriteRegStr HKCU "Software\trx_vanity" "" $INSTDIR
SectionEnd

; Uninstaller
Section "Uninstall"
    Delete "$INSTDIR\trx_vanity.exe"
    Delete "$INSTDIR\trx_vanity_gui.bat"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$INSTDIR\share"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\TRX Vanity\TRX Vanity (GUI Launcher).lnk"
    Delete "$SMPROGRAMS\TRX Vanity\TRX Vanity CLI.lnk"
    Delete "$SMPROGRAMS\TRX Vanity\Uninstall.lnk"
    RMDir "$SMPROGRAMS\TRX Vanity"
    Delete "$DESKTOP\TRX Vanity.lnk"

    DeleteRegKey HKCU "Software\trx_vanity"
SectionEnd
