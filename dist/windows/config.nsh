;Start of user configurable options
;==============================================================================

; Uncomment if you want to use UPX to pack the installer header
; Doing so may make antivirus software flag the installer as virus/malware
;!define USE_UPX

; qBittorrent version
; The string MUST contain ONLY numbers delimited by dots.
; It MUST contain a maximum of 4 delimited numbers
; Other values will result in undefined behavior
; examples:
; 4.5.0 -> good
; 4.5.1.3 -> good
; 4.5.1.3.2 -> bad
; 4.5.0beta -> bad
!define /ifndef QBT_VERSION "5.2.0"

; Option that controls the installer's window name
; If set, its value will be used like this:
; "qBittorrent ${QBT_INSTALLER_FILENAME}"
; If not set, the window name will be auto composed from QBT_VERSION
; If you set this define then you MUST set QBT_INSTALLER_FILENAME too. Otherwise it will be ignored.
; This define is meant to ease automation from scripts/commandline
;!define QBT_INSTALLER_WINDOWNAME

; Option that controls the installer's window name
; If set, its value will be used like this:
; "qbittorrent_${QBT_INSTALLER_FILENAME}_setup.exe"
; If not set, the installer filename will be auto composed from QBT_VERSION
; If you set this define then you MUST set QBT_INSTALLER_WINDOWNAME too. Otherwise it will be ignored.
; This define is meant to ease automation from scripts/commandline
;!define QBT_INSTALLER_FILENAME

;End of user configurable options
;==============================================================================

!ifndef QBT_INSTALLER_WINDOWNAME | QBT_INSTALLER_FILENAME
  ; The name of the installer
  !define QBT_INSTALLER_WINDOWNAME "${QBT_VERSION} x64"

  ; The file to write
  !define QBT_INSTALLER_FILENAME "${QBT_VERSION}_x64"
!endif

!define /ifndef QBT_DIST_DIR "qBittorrent"
!define /ifndef QBT_NSIS_PLUGINS_DIR "NSISPlugins"

Unicode true
ManifestDPIAware true

!ifdef USE_UPX
!packhdr "$%TEMP%\exehead.tmp" 'upx.exe -9 --best --ultra-brute "$%TEMP%\exehead.tmp"'
!endif

;Setting the compression
SetCompressor /SOLID LZMA
SetCompressorDictSize 64
XPStyle on

!include "MUI2.nsh"
!include "UAC.nsh"
!include "FileFunc.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!include "3rdparty\VersionCompleteXXXX.nsi"

;For the file association
!define SHCNE_ASSOCCHANGED 0x8000000
!define SHCNF_IDLIST 0

;For special folder detection
!define CSIDL_APPDATA '0x1A' ;Application Data path
!define CSIDL_LOCALAPPDATA '0x1C' ;Local Application Data path

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION PageFinishRun
!define MUI_FINISHPAGE_RUN_TEXT $(launch_qbt)

; The name of the installer
Name "qBittorrent ${QBT_INSTALLER_WINDOWNAME}"

; The file to write
OutFile "qbittorrent_${QBT_INSTALLER_FILENAME}_setup.exe"

;Installer Version Information
VIAddVersionKey "ProductName" "qBittorrent"
VIAddVersionKey "CompanyName" "The qBittorrent project"
VIAddVersionKey "LegalCopyright" "Copyright ©2006-2024 The qBittorrent project"
VIAddVersionKey "FileDescription" "qBittorrent - A Bittorrent Client"
VIAddVersionKey "FileVersion" "${QBT_VERSION}"

; VIProductVersion needs a 4 part version.
; If QBT_VERSION contains less than 4 parts then VersionCompleteXXXX, will extend it with zeroes.
${VersionCompleteXXXX} ${QBT_VERSION} VERSION_4_PART
VIProductVersion "${VERSION_4_PART}"

; The default installation directory.
InstallDir $PROGRAMFILES64\qBittorrent

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM Software\qbittorrent InstallLocation

; Request application privileges for Windows Vista
RequestExecutionLevel user

;--------------------------------
;General Settings
!define MUI_ABORTWARNING
!define MUI_HEADERIMAGE
!define MUI_COMPONENTSPAGE_NODESC
;!define MUI_ICON "qbittorrent.ico"
!define MUI_LICENSEPAGE_CHECKBOX
!define MUI_LANGDLL_ALLLANGUAGES

;--------------------------------
;Remember the uninstaller/installer language
!define MUI_LANGDLL_REGISTRY_ROOT "HKLM"
!define MUI_LANGDLL_REGISTRY_KEY "Software\qbittorrent"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

;--------------------------------
;Installer Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_COMPONENTS
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE check_instance
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

;--------------------------------
;Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE un.check_instance
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_RESERVEFILE_LANGDLL

!addplugindir /x86-unicode "${QBT_NSIS_PLUGINS_DIR}"
ReserveFile /plugin FindProcDLL.dll
ReserveFile /plugin UAC.dll

!macro Init thing
uac_tryagain:
!insertmacro UAC_RunElevated
${Switch} $0
${Case} 0
	${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
	${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
	${If} $1 = 3 ;RunAs completed successfully, but with a non-admin user
		MessageBox mb_YesNo|mb_IconExclamation|mb_TopMost|mb_SetForeground "This ${thing} requires admin privileges, try again" /SD IDNO IDYES uac_tryagain IDNO 0
	${EndIf}
	;fall-through and die
${Case} 1223
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "This ${thing} requires admin privileges, aborting!"
	Quit
${Case} 1062
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Logon service not running, aborting!"
	Quit
${Default}
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Unable to elevate , error $0"
	Quit
${EndSwitch}

SetShellVarContext all
!macroend
