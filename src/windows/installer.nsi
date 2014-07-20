Section "-hidden"

    ;Search if qBittorrent is already installed.
    FindFirst $0 $1 "$INSTDIR\uninst.exe"
    FindClose $0
    StrCmp $1 "" done
        
    ;Run the uninstaller of the previous install.
    DetailPrint $(inst_unist)    
    ExecWait '"$INSTDIR\uninst.exe" /S _?=$INSTDIR'
    Delete "$INSTDIR\uninst.exe"
    
  
    done:

SectionEnd


Section $(inst_qbt_req) ;"qBittorrent (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ;Create 'translations' directory
  CreateDirectory $INSTDIR\translations
  
  ; Put file there  
  File "qbittorrent.exe"
  File "qbittorrent.pdb"
  File "qt.conf"
  File /oname=translations\qt_ar.qm "translations\qt_ar.qm"
  File /oname=translations\qt_bg.qm "translations\qt_bg.qm"
  File /oname=translations\qt_ca.qm "translations\qt_ca.qm"
  File /oname=translations\qt_cs.qm "translations\qt_cs.qm"
  File /oname=translations\qt_da.qm "translations\qt_da.qm"
  File /oname=translations\qt_de.qm "translations\qt_de.qm"
  File /oname=translations\qt_es.qm "translations\qt_es.qm"
  File /oname=translations\qt_eu.qm "translations\qt_eu.qm"  
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
  WriteRegStr HKLM "Software\qBittorrent" "InstallLocation" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "DisplayName" "qBittorrent ${PROG_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "UninstallString" '"$INSTDIR\uninst.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "DisplayIcon" '"$INSTDIR\qbittorrent.exe",0'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "Publisher" "The qBittorrent project"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "URLInfoAbout" "http://www.qbittorrent.org"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "DisplayVersion" "${PROG_VERSION}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "NoRepair" 1
  WriteUninstaller "uninst.exe"  
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent" "EstimatedSize" "$0"
  
  ; qBittorrent ProgID
  WriteRegStr HKLM "Software\Classes\qBittorrent" "" "qBittorrent Torrent File"
  WriteRegStr HKLM "Software\Classes\qBittorrent" "FriendlyTypeName" "qBittorrent Torrent File"
  WriteRegStr HKLM "Software\Classes\qBittorrent\shell" "" "open"
  WriteRegStr HKLM "Software\Classes\qBittorrent\shell\open\command" "" '"$INSTDIR\qbittorrent.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\qBittorrent\DefaultIcon" "" '"$INSTDIR\qbittorrent.exe",1'
  
SectionEnd

; Optional section (can be disabled by the user)
Section /o $(inst_dekstop) ;"Create Desktop Shortcut"

  CreateShortCut "$DESKTOP\qBittorrent.lnk" "$INSTDIR\qbittorrent.exe"
  
SectionEnd

Section $(inst_startmenu) ;"Create Start Menu Shortcut"

  CreateDirectory "$SMPROGRAMS\qBittorrent"  
  CreateShortCut "$SMPROGRAMS\qBittorrent\qBittorrent.lnk" "$INSTDIR\qbittorrent.exe"
  CreateShortCut "$SMPROGRAMS\qBittorrent\Uninstall.lnk" "$INSTDIR\uninst.exe"
  
SectionEnd

Section $(inst_firewall)

  DetailPrint $(inst_firewallinfo)
  nsisFirewallW::AddAuthorizedApplication "$INSTDIR\qbittorrent.exe" "qBittorrent" 
    
SectionEnd

;--------------------------------

Function .onInit    
    
    !insertmacro MUI_LANGDLL_DISPLAY
	
FunctionEnd

Function check_instance

    check:
    FindProcDLL::FindProc "qbittorrent.exe"
    StrCmp $R0 "1" 0 notfound
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION $(inst_warning) IDRETRY check IDCANCEL done

    done:
    Abort
    
    notfound:
    
FunctionEnd
