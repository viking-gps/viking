; Installer script for win32 Viking
; Based on Win32 Pidgin installer by Herman Bloggs <hermanator12002@yahoo.com>
; and Daniel Atallah <daniel_atallah@yahoo.com>
; Heavily modified for Viking by Mathieu Albinet <mathieu_a@users.sourceforge.net>

;--------------------------------
;Global Variables
Var name

;--------------------------------
;Configuration
;Needs to be 4 numbers:  W.X.Y.Z
!define VIKING_VERSION  "1.4.2.0"

;The name var is set in .onInit
Name $name

OutFile "viking-${VIKING_VERSION}.exe"

SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
SetDateSave on

; $name and $INSTDIR are set in .onInit function..

!include "MUI.nsh"
!include "Sections.nsh"
!include "WinVer.nsh"
!include "LogicLib.nsh"
;; http://nsis.sourceforge.net/File_Association
!include "FileAssociation.nsh"

!include "FileFunc.nsh"
!insertmacro GetParameters
!insertmacro GetOptions
!insertmacro GetParent

!include "WordFunc.nsh"

;--------------------------------
;Defines

!define VIKING_NSIS_INCLUDE_PATH		"."

; Remove these and the stuff that uses them at some point
!define VIKING_REG_KEY				"SOFTWARE\viking"
!define VIKING_UNINSTALL_KEY			"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\viking"

!define HKLM_APP_PATHS_KEY			"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\viking.exe"
!define VIKING_UNINST_EXE			"viking-uninst.exe"

;--------------------------------
;Version resource
VIProductVersion "${VIKING_VERSION}"
VIAddVersionKey "ProductName" "Viking"
VIAddVersionKey "FileVersion" "${VIKING_VERSION}"
VIAddVersionKey "ProductVersion" "${VIKING_VERSION}"
VIAddVersionKey "LegalCopyright" ""
VIAddVersionKey "FileDescription" "Viking Installer"

;--------------------------------
;Modern UI Configuration

  !define MUI_ICON				".\pixmaps\viking_icon.ico"
  !define MUI_UNICON				".\pixmaps\viking_icon.ico"
;  !define MUI_WELCOMEFINISHPAGE_BITMAP		".\pixmaps\viking-intro.bmp"
;  !define MUI_HEADERIMAGE
;  !define MUI_HEADERIMAGE_BITMAP		".\pixmaps\viking-header.bmp"

  ; Alter License section
  !define MUI_LICENSEPAGE_BUTTON		$(VIKING_LICENSE_BUTTON)
  !define MUI_LICENSEPAGE_TEXT_BOTTOM		$(VIKING_LICENSE_BOTTOM_TEXT)

  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU"
  !define MUI_LANGDLL_REGISTRY_KEY ${VIKING_REG_KEY}
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

  !define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_ABORTWARNING

  ;Finish Page config
  !define MUI_FINISHPAGE_NOAUTOCLOSE
  !define MUI_FINISHPAGE_RUN			"$INSTDIR\viking.exe"
  !define MUI_FINISHPAGE_RUN_NOTCHECKED
  !define MUI_FINISHPAGE_LINK			$(VIKING_FINISH_VISIT_WEB_SITE)
  !define MUI_FINISHPAGE_LINK_LOCATION		"http://viking.sourceforge.net"

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE			".\bin\COPYING_GPL.txt"
  !insertmacro MUI_PAGE_COMPONENTS

  ; Viking install dir page
  !insertmacro MUI_PAGE_DIRECTORY

  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  ;; English goes first because its the default. The rest are
  ;; in alphabetical order (at least the strings actually displayed
  ;; will be).

  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "Spanish"

;--------------------------------
;Translations

  !define VIKING_DEFAULT_LANGFILE "${VIKING_NSIS_INCLUDE_PATH}\translations\english.nsh"

  !include "${VIKING_NSIS_INCLUDE_PATH}\langmacros.nsh"

  !insertmacro VIKING_MACRO_INCLUDE_LANGFILE "ENGLISH"		"${VIKING_NSIS_INCLUDE_PATH}\translations\english.nsh"
  !insertmacro VIKING_MACRO_INCLUDE_LANGFILE "FRENCH"		"${VIKING_NSIS_INCLUDE_PATH}\translations\french.nsh"
  !insertmacro VIKING_MACRO_INCLUDE_LANGFILE "SPANISH"		"${VIKING_NSIS_INCLUDE_PATH}\translations\spanish.nsh"

;--------------------------------
;Reserve Files
  ; Only need this if using bzip2 compression

  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS
  !insertmacro MUI_RESERVEFILE_LANGDLL
  ReserveFile "${NSISDIR}\Plugins\UserInfo.dll"


;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Start Install Sections ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;--------------------------------
;Uninstall any old version of Viking

Section -SecUninstallOldViking
  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0

  ;First try to uninstall Viking
  StrCpy $R4 ${VIKING_REG_KEY}
  StrCpy $R5 ${VIKING_UNINSTALL_KEY}
  StrCpy $R6 ${VIKING_UNINST_EXE}
  StrCpy $R7 "Viking"

  ;Determine user install rights
  StrCmp $R0 "HKLM" compare_hklm
  StrCmp $R0 "HKCU" compare_hkcu done

  compare_hkcu:
      ReadRegStr $R1 HKCU $R4 ""
      ReadRegStr $R2 HKCU $R4 "Version"
      ReadRegStr $R3 HKCU "$R5" "UninstallString"
      Goto try_uninstall

  compare_hklm:
      ReadRegStr $R1 HKLM $R4 ""
      ReadRegStr $R2 HKLM $R4 "Version"
      ReadRegStr $R3 HKLM "$R5" "UninstallString"

  ; If a previous version exists, remove it
  try_uninstall:
    StrCmp $R1 "" done
      StrCmp $R2 "" uninstall_problem
        ; Check if we have uninstall string..
        IfFileExists $R3 0 uninstall_problem
          ; Have uninstall string, go ahead and uninstall.
          SetOverwrite on
          ; Need to copy uninstaller outside of the install dir
          ClearErrors
          CopyFiles /SILENT $R3 "$TEMP\$R6"
          SetOverwrite off
          IfErrors uninstall_problem
            ; Ready to uninstall..
            ClearErrors
            ExecWait '"$TEMP\$R6" /S _?=$R1'
            IfErrors exec_error
              Delete "$TEMP\$R6"
            Goto done

            exec_error:
              Delete "$TEMP\$R6"
              Goto uninstall_problem

        uninstall_problem:
          ; We can't uninstall.  Either the user must manually uninstall or we ignore and reinstall over it.
          MessageBox MB_OKCANCEL $(VIKING_PROMPT_CONTINUE_WITHOUT_UNINSTALL) /SD IDOK IDOK done
          Quit
  done:
SectionEnd

;--------------------------------
;Viking Install Section

Section $(VIKING_SECTION_TITLE) SecViking
  SectionIn 1 RO

  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0
  StrCmp $R0 "NONE" viking_none
  StrCmp $R0 "HKLM" viking_hklm viking_hkcu

  ;Install rights for Local Machine
  viking_hklm:
    WriteRegStr HKLM "${HKLM_APP_PATHS_KEY}" "" "$INSTDIR\viking.exe"
    WriteRegStr HKLM "${HKLM_APP_PATHS_KEY}" "Path" "$R1\bin"
    WriteRegStr HKLM ${VIKING_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKLM ${VIKING_REG_KEY} "Version" "${VIKING_VERSION}"
    WriteRegStr HKLM "${VIKING_UNINSTALL_KEY}" "DisplayName" "Viking"
    WriteRegStr HKLM "${VIKING_UNINSTALL_KEY}" "DisplayVersion" "${VIKING_VERSION}"
    WriteRegStr HKLM "${VIKING_UNINSTALL_KEY}" "HelpLink" "http://sourceforge.net/apps/mediawiki/viking/"
    WriteRegDWORD HKLM "${VIKING_UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${VIKING_UNINSTALL_KEY}" "NoRepair" 1
    WriteRegStr HKLM "${VIKING_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\${VIKING_UNINST_EXE}"
    ; Sets scope of the desktop and Start Menu entries for all users.
    SetShellVarContext "all"
    Goto viking_install_files

    ;Install rights for Current User only 
  viking_hkcu:
    WriteRegStr HKCU ${VIKING_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKCU ${VIKING_REG_KEY} "Version" "${VIKING_VERSION}"
    WriteRegStr HKCU "${VIKING_UNINSTALL_KEY}" "DisplayName" "Viking"
    WriteRegStr HKCU "${VIKING_UNINSTALL_KEY}" "DisplayVersion" "${VIKING_VERSION}"
    WriteRegStr HKCU "${VIKING_UNINSTALL_KEY}" "HelpLink" "http://sourceforge.net/apps/mediawiki/viking/"
    WriteRegDWORD HKCU "${VIKING_UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${VIKING_UNINSTALL_KEY}" "NoRepair" 1
    WriteRegStr HKCU "${VIKING_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\${VIKING_UNINST_EXE}"
    Goto viking_install_files
  
  ;No install rights!
  viking_none:

  viking_install_files:
    SetOutPath "$INSTDIR"
    SetOverwrite on

    File /r .\bin\*.*

    ; If we don't have install rights we're done
    StrCmp $R0 "NONE" done
    SetOverwrite off

    ; write out uninstaller
    SetOverwrite on
    WriteUninstaller "$INSTDIR\${VIKING_UNINST_EXE}"
    SetOverwrite off

  done:
SectionEnd ; end of default Viking section

;--------------------------------
;Shortcuts

SectionGroup /e $(VIKING_SHORTCUTS_SECTION_TITLE) SecShortcuts
  ;Desktop shortcuts
  Section /o $(VIKING_DESKTOP_SHORTCUT_SECTION_TITLE) SecDesktopShortcut
    SetOverwrite on
    CreateShortCut "$DESKTOP\Viking.lnk" "$INSTDIR\viking.exe"
    SetOverwrite off
  SectionEnd
  ;Start menu shortcuts
  Section $(VIKING_STARTMENU_SHORTCUT_SECTION_TITLE) SecStartMenuShortcut
    SetOverwrite on
    CreateDirectory "$SMPROGRAMS\Viking"
    CreateShortCut "$SMPROGRAMS\Viking\Viking.lnk" "$INSTDIR\viking.exe"
    CreateShortCut "$SMPROGRAMS\Viking\User Manual.lnk" "$INSTDIR\viking.pdf"
    CreateShortCut "$SMPROGRAMS\Viking\Uninstall.lnk" "$INSTDIR\viking-uninst.exe"
    SetOverwrite off
  SectionEnd
SectionGroupEnd

;--------------------------------
;File association

Section $(VIKING_FILE_ASSOCIATION_SECTION_TITLE) SecFileAssociation
  ${registerExtension} "$INSTDIR\viking.exe" ".vik" "Viking File"
SectionEnd


;--------------------------------
;Uninstaller Section


Section Uninstall
  Call un.CheckUserInstallRights
  Pop $R0
  StrCmp $R0 "NONE" no_rights
  StrCmp $R0 "HKCU" try_hkcu try_hklm

  try_hkcu:
    ReadRegStr $R0 HKCU ${VIKING_REG_KEY} ""
    StrCmp $R0 $INSTDIR 0 cant_uninstall
      ; HKCU install path matches our INSTDIR so uninstall
      DeleteRegKey HKCU ${VIKING_REG_KEY}
      DeleteRegKey HKCU "${VIKING_UNINSTALL_KEY}"
      Goto cont_uninstall

  try_hklm:
    ReadRegStr $R0 HKLM ${VIKING_REG_KEY} ""
    StrCmp $R0 $INSTDIR 0 try_hkcu
      ; HKLM install path matches our INSTDIR so uninstall
      DeleteRegKey HKLM ${VIKING_REG_KEY}
      DeleteRegKey HKLM "${VIKING_UNINSTALL_KEY}"
      DeleteRegKey HKLM "${HKLM_APP_PATHS_KEY}"
      ; Sets start menu and desktop scope to all users..
      SetShellVarContext "all"

  cont_uninstall:

    ;Simply wipe the Viking install dir
    RMDir /r "$INSTDIR"

    ; Shortcuts..
    Delete "$DESKTOP\Viking.lnk"

    ; File association
    ${unregisterExtension} ".vik" "Viking File"

    Goto done

  cant_uninstall:
    MessageBox MB_OK $(un.VIKING_UNINSTALL_ERROR_1) /SD IDOK
    Quit

  no_rights:
    MessageBox MB_OK $(un.VIKING_UNINSTALL_ERROR_2) /SD IDOK
    Quit

  done:
SectionEnd ; end of uninstall section

;--------------------------------
;Descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN

  !insertmacro MUI_DESCRIPTION_TEXT ${SecViking} \
        $(VIKING_SECTION_DESCRIPTION)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} \
        $(VIKING_SHORTCUTS_SECTION_DESCRIPTION)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktopShortcut} \
        $(VIKING_DESKTOP_SHORTCUT_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenuShortcut} \
        $(VIKING_STARTMENU_SHORTCUT_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecFileAssociation} \
        $(VIKING_FILE_ASSOCIATION_DESC)

!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Functions

;Macro to determine user install rights
;Will be used to determine where to install the program, shortcuts, ...
!macro CheckUserInstallRightsMacro UN
Function ${UN}CheckUserInstallRights
  Push $0
  Push $1
  ClearErrors
  UserInfo::GetName
  IfErrors Win9x
  Pop $0
  UserInfo::GetAccountType
  Pop $1

  StrCmp $1 "Admin" 0 +3
    StrCpy $1 "HKLM"
    Goto done
  StrCmp $1 "Power" 0 +3
    StrCpy $1 "HKLM"
    Goto done
  StrCmp $1 "User" 0 +3
    StrCpy $1 "HKCU"
    Goto done
  StrCmp $1 "Guest" 0 +3
    StrCpy $1 "NONE"
    Goto done
  ; Unknown error
  StrCpy $1 "NONE"
  Goto done

  Win9x:
    StrCpy $1 "HKLM"

  done:
    Exch $1
    Exch
    Pop $0
FunctionEnd
!macroend
!insertmacro CheckUserInstallRightsMacro ""
!insertmacro CheckUserInstallRightsMacro "un."

;Macro to determine if Viking is running before installation/unistallation
!macro RunCheckMacro UN
Function ${UN}RunCheck
  FindProcDLL::FindProc "viking.exe"
  IntCmp $R0 1 0 notRunning
    MessageBox MB_OK|MB_ICONEXCLAMATION $(VIKING_IS_RUNNING) /SD IDOK
    Abort
  notRunning:
FunctionEnd
!macroend

!insertmacro RunCheckMacro ""
!insertmacro RunCheckMacro "un."

;Installer extra configuration at execution time: language, path, ...
Function .onInit
  ;Check if viking installer is already running
  Push $R0
  Push $R1
  Push $R2

  ;Check if viking is running
  Call RunCheck
  StrCpy $name "Viking ${VIKING_VERSION}"

  ClearErrors
  ;Make sure that there was a previous installation
  ReadRegStr $R0 HKCU "${VIKING_REG_KEY}" "Installer Language"
  
  ;Preselect the "shortcuts" checkboxes according to the previous installation
  !insertmacro SelectSection ${SecDesktopShortcut}
  !insertmacro selectSection ${SecStartMenuShortcut}
  
  ;Read command line parameters
  
  ;Read language command line parameters
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} "$R0" "/L=" $R1
  IfErrors +3
  StrCpy $LANGUAGE $R1
  Goto skip_lang

  ; Select Language
    ; Display Language selection dialog
    !insertmacro MUI_LANGDLL_DISPLAY
    skip_lang:

  ;Read desktop shortcut command line options
  ClearErrors
  ${GetOptions} "$R0" "/DS=" $R1
  IfErrors +8
  SectionGetFlags ${SecDesktopShortcut} $R2
  StrCmp "1" $R1 0 +2
  IntOp $R2 $R2 | ${SF_SELECTED}
  StrCmp "0" $R1 0 +3
  IntOp $R1 ${SF_SELECTED} ~
  IntOp $R2 $R2 & $R1
  SectionSetFlags ${SecDesktopShortcut} $R2

  ;Read start menu shortcuts command line options
  ClearErrors
  ${GetOptions} "$R0" "/SMS=" $R1
  IfErrors +8
  SectionGetFlags ${SecStartMenuShortcut} $R2
  StrCmp "1" $R1 0 +2
  IntOp $R2 $R2 | ${SF_SELECTED}
  StrCmp "0" $R1 0 +3
  IntOp $R1 ${SF_SELECTED} ~
  IntOp $R2 $R2 & $R1
  SectionSetFlags ${SecStartMenuShortcut} $R2

  ; If install path was set on the command, use it.
  StrCmp $INSTDIR "" 0 instdir_done

  ;  If viking is currently installed, we should default to where it is currently installed
  ClearErrors
  ReadRegStr $INSTDIR HKCU "${VIKING_REG_KEY}" ""
  IfErrors +2
  StrCmp $INSTDIR "" 0 instdir_done
  ClearErrors
  ReadRegStr $INSTDIR HKLM "${VIKING_REG_KEY}" ""
  IfErrors +2
  StrCmp $INSTDIR "" 0 instdir_done

  Call CheckUserInstallRights
  Pop $R0

  StrCmp $R0 "HKLM" 0 user_dir
    StrCpy $INSTDIR "$PROGRAMFILES\Viking"
    Goto instdir_done
  user_dir:
    Push $SMPROGRAMS
    ${GetParent} $SMPROGRAMS $R2
    ${GetParent} $R2 $R2
    StrCpy $INSTDIR "$R2\Viking"

  instdir_done:
;LogSet on
  Pop $R2
  Pop $R1
  Pop $R0
FunctionEnd

Function un.onInit
  ;Check if viking is running
  Call un.RunCheck
  StrCpy $name "Viking ${VIKING_VERSION}"

  ; Get stored language preference
  !insertmacro MUI_UNGETLANGUAGE

FunctionEnd
