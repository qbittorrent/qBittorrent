;Start of user configurable options
;==============================================================================

; Uncomment if you want to use UPX to pack the installer header
; Doing so may make antivirus software flag the installer as virus/malware
;!define USE_UPX

; Uncomment when packaging 64bit qbittorrent
;!define QBT_IS_X64

; Uncomment when packaging qt6 qbittorrent
; It will also define QBT_IS_X64
;!define QBT_USES_QT6

!ifdef QBT_USES_QT6
!define /redef QBT_IS_X64
!endif

; qBittorrent version
; The string MUST contain ONLY numbers delimited by dots.
; It MUST contain a maximum of 4 delimited numbers
; Other values will result in undefined behavior
; examples:
; 4.5.0 -> good
; 4.5.1.3 -> good
; 4.5.1.3.2 -> bad
; 4.5.0beta -> bad
!define /ifndef QBT_VERSION "4.5.0"

; Option that controls the installer's window name
; If set, its value will be used like this:
; "qBittorrent ${QBT_INSTALLER_FILENAME}"
; If not set, the window name will be auto composed from QBT_VERSION, QBT_USES_QT6, QBT_IS_X64
; If you set this define then you MUST set QBT_INSTALLER_FILENAME too. Otherwise it will be ignored.
; This define is meant to ease automation from scripts/commandline
;!define QBT_INSTALLER_WINDOWNAME

; Option that controls the installer's window name
; If set, its value will be used like this:
; "qbittorrent_${QBT_INSTALLER_FILENAME}_setup.exe"
; If not set, the window name will be auto composed from QBT_VERSION, QBT_USES_QT6, QBT_IS_X64
; If you set this define then you MUST set QBT_INSTALLER_WINDOWNAME too. Otherwise it will be ignored.
; This define is meant to ease automation from scripts/commandline
;!define QBT_INSTALLER_FILENAME

;End of user configurable options
;==============================================================================

!ifndef QBT_INSTALLER_WINDOWNAME | QBT_INSTALLER_FILENAME
  !ifndef QBT_IS_X64
    ; The name of the installer
    !define QBT_INSTALLER_WINDOWNAME "${QBT_VERSION}"

    ; The file to write
    !define QBT_INSTALLER_FILENAME "${QBT_VERSION}"
  !else ; QBT_IS_X64
    !ifndef QBT_USES_QT6
      ; The name of the installer
      !define QBT_INSTALLER_WINDOWNAME "${QBT_VERSION} x64"

      ; The file to write
      !define QBT_INSTALLER_FILENAME "${QBT_VERSION}_x64"
    !else ; QBT_USES_QT6
      ; The name of the installer
      !define QBT_INSTALLER_WINDOWNAME "${QBT_VERSION} (qt6) x64"

      ; The file to write
      !define QBT_INSTALLER_FILENAME "${QBT_VERSION}_qt6_x64"
    !endif ; QBT_USES_QT6
  !endif ; QBT_IS_X64
!endif

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
!ifdef QBT_IS_X64
!include "x64.nsh"
!endif
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
VIAddVersionKey "LegalCopyright" "Copyright ©2006-2022 The qBittorrent project"
VIAddVersionKey "FileDescription" "qBittorrent - A Bittorrent Client"
VIAddVersionKey "FileVersion" "${QBT_VERSION}"

; VIProductVersion needs a 4 part version.
; If QBT_VERSION contains less than 4 parts then VersionCompleteXXXX, will extend it with zeroes.
${VersionCompleteXXXX} ${QBT_VERSION} VERSION_4_PART
VIProductVersion "${VERSION_4_PART}"

; The default installation directory. It changes depending if we install in the 64bit dir or not.
; A caveat of this is if a user has installed a 32bit version and then runs the 64bit installer
; (which in turn launches the 32bit uninstaller first) the value will still point to the 32bit location.
; The user has to manually uninstall the old version and THEN run the 64bit installer
!ifndef QBT_IS_X64
  InstallDir $PROGRAMFILES32\qBittorrent
!else
  InstallDir $PROGRAMFILES64\qBittorrent
!endif

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
;Remember the unistaller/installer language
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
ReserveFile "${NSISDIR}\Plugins\x86-unicode\FindProcDLL.dll"
ReserveFile "${NSISDIR}\Plugins\x86-unicode\UAC.dll"

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
