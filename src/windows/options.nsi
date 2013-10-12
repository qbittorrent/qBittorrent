Unicode true
;Compress the header too
!packhdr "$%TEMP%\exehead.tmp" 'upx.exe -9 --best --ultra-brute "$%TEMP%\exehead.tmp"'

;Setting the compression
SetCompressor /SOLID LZMA
SetCompressorDictSize 64
XPStyle on

!include "MUI.nsh" 
!include "FileFunc.nsh"

;For the file association
!define SHCNE_ASSOCCHANGED 0x8000000
!define SHCNF_IDLIST 0

;For special folder detection
!define CSIDL_APPDATA '0x1A' ;Application Data path
!define CSIDL_LOCALAPPDATA '0x1C' ;Local Application Data path

!define PROG_VERSION "3.2.0"
!define MUI_FINISHPAGE_RUN "$INSTDIR\qbittorrent.exe"
!define MUI_FINISHPAGE_RUN_TEXT $(launch_qbt)

; The name of the installer
Name "qBittorrent ${PROG_VERSION}"

; The file to write
OutFile "qbittorrent_${PROG_VERSION}_setup.exe"

;Installer Version Information
VIAddVersionKey "ProductName" "qBittorrent"
VIAddVersionKey "CompanyName" "The qBittorrent project"
VIAddVersionKey "LegalCopyright" "Copyright ©2006-2013 The qBittorrent project"
VIAddVersionKey "FileDescription" "qBittorrent - A Bittorrent Client"
VIAddVersionKey "FileVersion" "${PROG_VERSION}"

VIProductVersion "${PROG_VERSION}.0"

; The default installation directory
InstallDir $PROGRAMFILES\qBittorrent

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM Software\qbittorrent InstallLocation

; Request application privileges for Windows Vista
RequestExecutionLevel admin

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
