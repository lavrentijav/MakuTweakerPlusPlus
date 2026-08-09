; MakuTweaker++ installer
;
; Packages the *normal* build - the executable plus its loc/, assets/ and
; previewimg/ folders. The single-file build needs no installer: it is already
; one self-contained file.
;
; Build with:
;   makensis /DSOURCE_DIR=<path to build\Release> /DAPP_VERSION=5.8.1 MakuTweaker.nsi

Unicode true
ManifestDPIAware true

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!ifndef SOURCE_DIR
  !error "SOURCE_DIR is required: makensis /DSOURCE_DIR=..\build\Release MakuTweaker.nsi"
!endif
!ifndef APP_VERSION
  !define APP_VERSION "5.8.0"
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "MakuTweaker++-Setup.exe"
!endif

!define APP_NAME     "MakuTweaker++"
!define APP_EXE      "MakuTweaker++.exe"
!define APP_CLI      "MakuTweaker++.com"
!define APP_PUBLISHER "lavrentijav"
!define APP_URL      "https://github.com/lavrentijav/MakuTweakerPlusPlus"
!define UNINST_KEY   "Software\Microsoft\Windows\CurrentVersion\Uninstall\MakuTweakerPlusPlus"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "InstallDir"

; Writes to Program Files, HKLM and the all-users Start menu.
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "5.8.0.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "FileDescription" "${APP_NAME} installer"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright ${APP_PUBLISHER}"

!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icons\MakuT.ico"
!define MUI_UNICON "..\assets\icons\MakuT.ico"

!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Run ${APP_NAME}"

!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
; The optional shortcut sections need a components page, otherwise their
; descriptions have nowhere to appear.
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "${APP_NAME} requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64

  ; Offer to clean up a previous version rather than layering files on top.
  ReadRegStr $R0 HKLM "${UNINST_KEY}" "UninstallString"
  ${If} $R0 != ""
    MessageBox MB_YESNO|MB_ICONQUESTION \
      "${APP_NAME} is already installed.$\n$\nRemove the previous version first?" \
      IDNO +2
    ExecWait '$R0 /S _?=$INSTDIR'
  ${EndIf}
FunctionEnd

Section "MakuTweaker++" SecMain
  SectionIn RO

  ; A running copy keeps its own .exe locked, which would abort the install
  ; halfway and leave a half-written folder behind. Deleting the old file is a
  ; plug-in-free way to detect that: on Windows the delete simply fails while
  ; the image is mapped.
  retry_locked:
    ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
      Delete "$INSTDIR\${APP_EXE}"
      ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
        MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
          "${APP_NAME} appears to be running.$\n$\nClose it and press Retry." \
          IDRETRY retry_locked
        Abort
      ${EndIf}
    ${EndIf}

  SetOutPath "$INSTDIR"
  SetOverwrite on

  File "${SOURCE_DIR}\${APP_EXE}"
  ; The console front end is what makes `MakuTweaker++ <command>` behave like a
  ; normal CLI tool; the GUI works without it, so do not fail if it is missing.
  File /nonfatal "${SOURCE_DIR}\${APP_CLI}"

  SetOutPath "$INSTDIR\loc"
  File /r "${SOURCE_DIR}\loc\*.*"
  SetOutPath "$INSTDIR\assets"
  File /r "${SOURCE_DIR}\assets\*.*"
  SetOutPath "$INSTDIR\previewimg"
  File /nonfatal /r "${SOURCE_DIR}\previewimg\*.*"

  SetOutPath "$INSTDIR"
  File "..\LICENSE"
  File "..\README.md"

  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\${APP_NAME}" "InstallDir" "$INSTDIR"

  ; Lets Windows find the program by name from Win+R and the command prompt.
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\${APP_EXE}" \
    "" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\${APP_EXE}" \
    "Path" "$INSTDIR"

  ; Add or remove programs entry.
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName"     "${APP_NAME}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion"  "${APP_VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon"     "$INSTDIR\${APP_EXE},0"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher"       "${APP_PUBLISHER}"
  WriteRegStr HKLM "${UNINST_KEY}" "URLInfoAbout"    "${APP_URL}"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Start menu shortcut" SecStartMenu
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" \
    "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The application and its data files."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Add ${APP_NAME} to the Start menu."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Put a shortcut on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Uninstall"
  ; The background metrics service, if the user ever enabled it, keeps the
  ; executable locked and would survive the uninstall as a broken service.
  nsExec::Exec '"$INSTDIR\${APP_EXE}" --uninstall-metrics-service'
  Pop $0

  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\${APP_CLI}"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"

  RMDir /r "$INSTDIR\loc"
  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\previewimg"
  RMDir "$INSTDIR"

  Delete "$DESKTOP\${APP_NAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APP_NAME}"

  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\${APP_EXE}"
  DeleteRegKey HKLM "Software\${APP_NAME}"

  ; Settings in %AppData% are intentionally left in place so a reinstall keeps
  ; the user's configuration.
SectionEnd
