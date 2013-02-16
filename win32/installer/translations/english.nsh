;;
;;  english.nsh
;;
;;  Default language strings for the Windows Viking NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro VIKING_MACRO_DEFAULT_STRING'
;;  with '!define'.

; Make sure to update the VIKING_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!insertmacro VIKING_MACRO_DEFAULT_STRING INSTALLER_IS_RUNNING			"The installer is already running."
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_IS_RUNNING			"An instance of Viking is currently running.  Please exit Viking and try again."

; License Page
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_LICENSE_BUTTON			"Next >"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_LICENSE_BOTTOM_TEXT		"$(^Name) is released under the GNU General Public License (GPL). The license is provided here for information purposes only. $_CLICK"

; Components Page
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_SECTION_TITLE			"Viking GPS data editor and analyzer (required)"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_SHORTCUTS_SECTION_TITLE		"Shortcuts"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Menu"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_FILE_ASSOCIATION_SECTION_TITLE	"File association"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_SECTION_DESCRIPTION		"Core Viking files and dlls"
!insertmacro VIKING_MACRO_DEFAULT_STRING GTK_SECTION_DESCRIPTION		"A multi-platform GUI toolkit, used by Viking"

!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_SHORTCUTS_SECTION_DESCRIPTION	"Shortcuts for starting Viking"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_DESKTOP_SHORTCUT_DESC		"Create a shortcut to Viking on the Desktop"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_STARTMENU_SHORTCUT_DESC		"Create a Start Menu entry for Viking"
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_FILE_ASSOCIATION_DESC	  "Associate .vik files with Viking"

; Installer Finish Page
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_FINISH_VISIT_WEB_SITE		"Visit the Viking Web Page"

; Viking Section Prompts and Texts
!insertmacro VIKING_MACRO_DEFAULT_STRING VIKING_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Unable to uninstall the currently installed version of Viking. The new version will be installed without removing the currently installed version."

; Uninstall Section Prompts
!insertmacro VIKING_MACRO_DEFAULT_STRING un.VIKING_UNINSTALL_ERROR_1		"The uninstaller could not find registry entries for Viking.$\rIt is likely that another user installed this application."
!insertmacro VIKING_MACRO_DEFAULT_STRING un.VIKING_UNINSTALL_ERROR_2		"You do not have permission to uninstall this application."
