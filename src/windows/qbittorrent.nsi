;Compress the header too
!packhdr "$%TEMP%\exehead.tmp" 'upx.exe -9 --best --ultra-brute "$%TEMP%\exehead.tmp"'

;Setting the compression
SetCompressor /SOLID LZMA
SetCompressorDictSize 64
XPStyle on

!include "MUI.nsh" 

;For the file association
!define SHCNE_ASSOCCHANGED 0x8000000
!define SHCNF_IDLIST 0

;For special folder detection
!define CSIDL_APPDATA '0x1A' ;Application Data path
!define CSIDL_LOCALAPPDATA '0x1C' ;Local Application Data path

!define PROG_VERSION "3.0.3"

; The name of the installer
Name "qBittorrent"

; The file to write
OutFile "qbittorrent_${PROG_VERSION}_setup.exe"

; The default installation directory
InstallDir $PROGRAMFILES\qBittorrent

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM Software\qbittorrent InstallLocation

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages
;Interface Settings
!define MUI_ABORTWARNING
!define MUI_HEADERIMAGE
!define MUI_COMPONENTSPAGE_NODESC
;!define MUI_ICON "qbittorrent.ico"
!define MUI_LICENSEPAGE_CHECKBOX
!define MUI_LANGDLL_ALLLANGUAGES

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES



!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Afrikaans"
!insertmacro MUI_LANGUAGE "Albanian"
!insertmacro MUI_LANGUAGE "Arabic"
!insertmacro MUI_LANGUAGE "Basque"
!insertmacro MUI_LANGUAGE "Belarusian"
!insertmacro MUI_LANGUAGE "Bosnian"
!insertmacro MUI_LANGUAGE "Breton"
!insertmacro MUI_LANGUAGE "Bulgarian"
!insertmacro MUI_LANGUAGE "Catalan"
!insertmacro MUI_LANGUAGE "Croatian"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Danish"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "Esperanto"
!insertmacro MUI_LANGUAGE "Estonian"
!insertmacro MUI_LANGUAGE "Farsi"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Galician"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Greek"
!insertmacro MUI_LANGUAGE "Hebrew"
!insertmacro MUI_LANGUAGE "Hungarian"
!insertmacro MUI_LANGUAGE "Icelandic"
!insertmacro MUI_LANGUAGE "Indonesian"
!insertmacro MUI_LANGUAGE "Irish"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Kurdish"
!insertmacro MUI_LANGUAGE "Latvian"
!insertmacro MUI_LANGUAGE "Lithuanian"
!insertmacro MUI_LANGUAGE "Luxembourgish"
!insertmacro MUI_LANGUAGE "Macedonian"
!insertmacro MUI_LANGUAGE "Malay"
!insertmacro MUI_LANGUAGE "Mongolian"
!insertmacro MUI_LANGUAGE "Norwegian"
!insertmacro MUI_LANGUAGE "NorwegianNynorsk"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "Serbian"
!insertmacro MUI_LANGUAGE "SerbianLatin"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "Slovak"
!insertmacro MUI_LANGUAGE "Slovenian"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SpanishInternational"
!insertmacro MUI_LANGUAGE "Swedish"
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "Uzbek"
!insertmacro MUI_LANGUAGE "Welsh"


;--------------------------------

; The stuff to install
Section "qBittorrent (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ;Create 'translations' directory
  CreateDirectory $INSTDIR\translations
  
  ; Put file there  
  File "qbittorrent.exe"
  File "qt.conf"
  File /oname=translations\qt_ar.qm "translations\qt_ar.qm"
  File /oname=translations\qt_bg.qm "translations\qt_bg.qm"
  File /oname=translations\qt_ca.qm "translations\qt_ca.qm"
  File /oname=translations\qt_cs.qm "translations\qt_cs.qm"
  File /oname=translations\qt_da.qm "translations\qt_da.qm"
  File /oname=translations\qt_de.qm "translations\qt_de.qm"
  File /oname=translations\qt_es.qm "translations\qt_es.qm"  
  File /oname=translations\qt_fi.qm "translations\qt_fi.qm"
  File /oname=translations\qt_fr.qm "translations\qt_fr.qm"
  File /oname=translations\qt_gl.qm "translations\qt_gl.qm"
  File /oname=translations\qt_he.qm "translations\qt_he.qm"
  File /oname=translations\qt_hu.qm "translations\qt_hu.qm"
  File /oname=translations\qt_it.qm "translations\qt_it.qm"
  File /oname=translations\qt_ja.qm "translations\qt_ja.qm"
  File /oname=translations\qt_ko.qm "translations\qt_ko.qm"
  File /oname=translations\qt_lt.qm "translations\qt_lt.qm"
  File /oname=translations\qt_nl.qm "translations\qt_nl.qm"
  File /oname=translations\qt_pl.qm "translations\qt_pl.qm"
  File /oname=translations\qt_pt.qm "translations\qt_pt.qm"
  File /oname=translations\qt_pt_BR.qm "translations\qt_pt_BR.qm"
  File /oname=translations\qt_ru.qm "translations\qt_ru.qm"
  File /oname=translations\qt_sk.qm "translations\qt_sk.qm"  
  File /oname=translations\qt_sv.qm "translations\qt_sv.qm"
  File /oname=translations\qt_tr.qm "translations\qt_tr.qm"
  File /oname=translations\qt_uk.qm "translations\qt_uk.qm"
  File /oname=translations\qt_zh_CN.qm "translations\qt_zh_CN.qm"
  File /oname=translations\qt_zh_TW.qm "translations\qt_zh_TW.qm" 
  
  ; Write the installation path into the registry  
  WriteRegStr HKLM "Software\qbittorrent" "InstallLocation" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qbittorrent" "DisplayName" "qBittorrent ${PROG_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qbittorrent" "UninstallString" '"$INSTDIR\uninst.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qbittorrent" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qbittorrent" "NoRepair" 1
  WriteUninstaller "uninst.exe"  
  
SectionEnd

; Optional section (can be disabled by the user)
Section /o "Create Desktop Shortcut" 

  CreateShortCut "$DESKTOP\qBittorrent.lnk" "$INSTDIR\qbittorrent.exe"
  
SectionEnd

Section "Create Start Menu Shortcut"  

  CreateDirectory "$SMPROGRAMS\qBittorrent"  
  CreateShortCut "$SMPROGRAMS\qBittorrent\qBittorrent.lnk" "$INSTDIR\qbittorrent.exe"
  CreateShortCut "$SMPROGRAMS\qBittorrent\Uninstall.lnk" "$INSTDIR\uninst.exe"
  
SectionEnd

Section "Open .torrent files with qBittorrent"

  WriteRegStr HKEY_CLASSES_ROOT ".torrent" "" "qBittorrent"
  WriteRegStr HKEY_CLASSES_ROOT ".torrent" "Content Type" "application/x-bittorrent"
  WriteRegStr HKEY_CLASSES_ROOT "qBittorrent\shell" "" "open"
  WriteRegStr HKEY_CLASSES_ROOT "qBittorrent\shell\open\command" "" '"$INSTDIR\qbittorrent.exe" "%1"'
  WriteRegStr HKEY_CLASSES_ROOT "qBittorrent\Content Type" "" "application/x-bittorrent"
  WriteRegStr HKEY_CLASSES_ROOT "qBittorrent\DefaultIcon" "" '"$INSTDIR\qbittorrent.exe",1'
  
  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, i 0, i 0)'
  
SectionEnd

Section "Open magnet links with qBittorrent"

  WriteRegStr HKEY_CLASSES_ROOT "Magnet" "" "Magnet URI"
  WriteRegStr HKEY_CLASSES_ROOT "Magnet" "Content Type" "application/x-magnet"
  WriteRegStr HKEY_CLASSES_ROOT "Magnet" "URL Protocol" ""
  WriteRegStr HKEY_CLASSES_ROOT "Magnet\DefaultIcon" "" '"$INSTDIR\qbittorrent.exe",1'
  WriteRegStr HKEY_CLASSES_ROOT "Magnet\shell" "" "open"
  WriteRegStr HKEY_CLASSES_ROOT "Magnet\shell\open\command" "" '"$INSTDIR\qbittorrent.exe" "%1"'
  
  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, i 0, i 0)'
  
SectionEnd

;--------------------------------

Function .onInit	
    ;Search if qBittorrent is already installed.
    FindFirst $0 $1 "$INSTDIR\uninst.exe"
    FindClose $0
    StrCmp $1 "" done
    
    ;Display a confirmation dialog
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
    "qBittorrent is already installed. $\n$\nClick $\"OK$\" to remove the \
    previous version or $\"Cancel$\" to cancel this upgrade." IDOK uninst
    Abort
    
    
    uninst:
    ;Run the uninstaller of the previous install.
	ExecWait '"$INSTDIR\uninst.exe" _?=$INSTDIR'
			
    done:
    !insertmacro MUI_LANGDLL_DISPLAY
	
FunctionEnd

;--------------------------------

; Uninstaller

Section "un.Remove files"
  SectionIn RO
  
; Remove files and uninstaller  
  Delete "$INSTDIR\qbittorrent.exe"
  Delete "$INSTDIR\qt.conf"  
  Delete "$INSTDIR\translations\qt_ar.qm"
  Delete "$INSTDIR\translations\qt_bg.qm"
  Delete "$INSTDIR\translations\qt_ca.qm"
  Delete "$INSTDIR\translations\qt_cs.qm"
  Delete "$INSTDIR\translations\qt_da.qm"
  Delete "$INSTDIR\translations\qt_de.qm"
  Delete "$INSTDIR\translations\qt_es.qm"
  Delete "$INSTDIR\translations\qt_fa.qm"
  Delete "$INSTDIR\translations\qt_fi.qm"
  Delete "$INSTDIR\translations\qt_fr.qm"
  Delete "$INSTDIR\translations\qt_gl.qm"
  Delete "$INSTDIR\translations\qt_he.qm"
  Delete "$INSTDIR\translations\qt_hu.qm"
  Delete "$INSTDIR\translations\qt_it.qm"
  Delete "$INSTDIR\translations\qt_ja.qm"
  Delete "$INSTDIR\translations\qt_ko.qm"
  Delete "$INSTDIR\translations\qt_lt.qm"
  Delete "$INSTDIR\translations\qt_nl.qm"
  Delete "$INSTDIR\translations\qt_pl.qm"
  Delete "$INSTDIR\translations\qt_pt.qm"
  Delete "$INSTDIR\translations\qt_pt_BR.qm"
  Delete "$INSTDIR\translations\qt_ru.qm"
  Delete "$INSTDIR\translations\qt_sk.qm"
  Delete "$INSTDIR\translations\qt_sl.qm"
  Delete "$INSTDIR\translations\qt_sv.qm"
  Delete "$INSTDIR\translations\qt_tr.qm"
  Delete "$INSTDIR\translations\qt_uk.qm"
  Delete "$INSTDIR\translations\qt_zh_CN.qm"
  Delete "$INSTDIR\translations\qt_zh_TW.qm"   
  Delete "$INSTDIR\uninst.exe"
  
  ; Remove directories used
  RMDir "$SMPROGRAMS\qBittorrent"
  RMDir "$INSTDIR\translations"
  RMDir "$INSTDIR"
SectionEnd

Section "un.Remove shortcuts"
  SectionIn RO
; Remove shortcuts, if any
  Delete "$SMPROGRAMS\qBittorrent\*.*"
  Delete "$DESKTOP\qBittorrent.lnk"
SectionEnd

Section "un.Remove file associations"
  SectionIn RO
  DeleteRegKey HKEY_CLASSES_ROOT ".torrent"
  DeleteRegKey HKEY_CLASSES_ROOT "qBittorrent"  
  DeleteRegKey HKEY_CLASSES_ROOT "Magnet"
  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, i 0, i 0)'
SectionEnd

Section "un.Remove registry keys" 
  SectionIn RO 
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qbittorrent"
  DeleteRegKey HKLM "Software\qbittorrent"
SectionEnd

Section /o "un.Remove configuration files"
  System::Call 'shell32::SHGetSpecialFolderPath(i $HWNDPARENT, t .r1, i ${CSIDL_APPDATA}, i0)i.r0'  
  RMDir /r "$1\qBittorrent"
  
  System::Call 'shell32::SHGetSpecialFolderPath(i $HWNDPARENT, t .r1, i ${CSIDL_LOCALAPPDATA}, i0)i.r0'
  RMDir /r "$1\qBittorrent\"
SectionEnd
