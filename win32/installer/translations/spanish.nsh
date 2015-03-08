;;  vim:syn=winbatch:fileencoding=cp1252:
;;
;;  spanish.nsh
;;
;;  Spanish language strings for the Windows Viking NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Roberto
;;
;; Modified for Viking by Mathieu Albinet <mathieu_a@users.sourceforge.net>

; Make sure to update the VIKING_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"El programa de instalación ya se está ejecutando."
!define VIKING_IS_RUNNING				"Una instancia de Viking se está ejecutando. Por favor, salga de Viking y vuelva a intentarlo."

; License Page
!define VIKING_LICENSE_BUTTON			"Siguiente >"
!define VIKING_LICENSE_BOTTOM_TEXT		"$(^Name) está disponible bajo licencia GNU General Public License (GPL). El siguiente texto de licencia es ofrecido únicamente a titulo informativo. $_CLICK" 

; Components Page
!define VIKING_SECTION_TITLE			"Viking, software de edición y de análisis de datos GPS (obligatorio)"
!define VIKING_SHORTCUTS_SECTION_TITLE		"Accesos directos"
!define VIKING_DESKTOP_SHORTCUT_SECTION_TITLE	"Escritorio"
!define VIKING_STARTMENU_SHORTCUT_SECTION_TITLE	"Menú Inicio"
!define VIKING_FILE_ASSOCIATION_SECTION_TITLE	"Extensión.gpx"
!define VIKING_SECTION_DESCRIPTION		"Ficheros y DLLs de base de Viking"
!define VIKING_GPSBABEL_SECTION_TITLE	"GPSBabel 1.5.2"

!define VIKING_SHORTCUTS_SECTION_DESCRIPTION	"Accesos directos para lanzar Viking"
!define VIKING_DESKTOP_SHORTCUT_DESC		"Crear un acceso directo a Viking en el escritorio"
!define VIKING_STARTMENU_SHORTCUT_DESC		"Crear un acceso directo a Viking en el menú de inicio"
!define VIKING_FILE_ASSOCIATION_DESC    "Asociar Viking con la extensión .gpx"
!define VIKING_INSTALL_GPSBABEL_DESC	"Instalar GPSBabel Programa"

; Installer Finish Page
!define VIKING_FINISH_VISIT_WEB_SITE		"Visite la página web de Viking" 

; Viking Section Prompts and Texts
!define VIKING_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Desinstalación de la versión actual de Viking imposible. La nueva versión será instalada sin suprimir la versión actual."

; Uninstall Section Prompts
!define un.VIKING_UNINSTALL_ERROR_1		"El programa de desinstalación no ha encontrado las entradas de Viking en la base de registros.$\rLa aplicación quizá haya sido instalada por un usuario distinto."
!define un.VIKING_UNINSTALL_ERROR_2		"Usted no tiene los permisos necesarios para suprimir esta aplicación."

!define VIKING_UNINSTALL_COMMENTS		"Edición y de análisis de datos GPS"
