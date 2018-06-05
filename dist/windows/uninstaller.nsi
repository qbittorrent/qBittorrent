Section "un.$(remove_files)" ;"un.Remove files"
  SectionIn RO

; Remove files and uninstaller
  Delete "$INSTDIR\qbittorrent.exe"
  Delete "$INSTDIR\qbittorrent.pdb"
  Delete "$INSTDIR\qt.conf"
  Delete "$INSTDIR\translations\qt_ar.qm"
  Delete "$INSTDIR\translations\qt_bg.qm"
  Delete "$INSTDIR\translations\qt_ca.qm"
  Delete "$INSTDIR\translations\qt_cs.qm"
  Delete "$INSTDIR\translations\qt_da.qm"
  Delete "$INSTDIR\translations\qt_de.qm"
  Delete "$INSTDIR\translations\qt_es.qm"
  Delete "$INSTDIR\translations\qt_eu.qm"
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
  Delete "$INSTDIR\translations\qt_pt_BR.qm"
  Delete "$INSTDIR\translations\qt_pt.qm"
  Delete "$INSTDIR\translations\qt_ru.qm"
  Delete "$INSTDIR\translations\qt_sk.qm"
  Delete "$INSTDIR\translations\qt_sl.qm"
  Delete "$INSTDIR\translations\qt_sv.qm"
  Delete "$INSTDIR\translations\qt_tr.qm"
  Delete "$INSTDIR\translations\qt_uk.qm"
  Delete "$INSTDIR\translations\qt_zh_CN.qm"
  Delete "$INSTDIR\translations\qt_zh_TW.qm"
  Delete "$INSTDIR\translations\qtbase_ca.qm"
  Delete "$INSTDIR\translations\qtbase_cs.qm"
  Delete "$INSTDIR\translations\qtbase_de.qm"
  Delete "$INSTDIR\translations\qtbase_fi.qm"
  Delete "$INSTDIR\translations\qtbase_fr.qm"
  Delete "$INSTDIR\translations\qtbase_he.qm"
  Delete "$INSTDIR\translations\qtbase_hu.qm"
  Delete "$INSTDIR\translations\qtbase_it.qm"
  Delete "$INSTDIR\translations\qtbase_ja.qm"
  Delete "$INSTDIR\translations\qtbase_ko.qm"
  Delete "$INSTDIR\translations\qtbase_lv.qm"
  Delete "$INSTDIR\translations\qtbase_pl.qm"
  Delete "$INSTDIR\translations\qtbase_ru.qm"
  Delete "$INSTDIR\translations\qtbase_sk.qm"
  Delete "$INSTDIR\translations\qtbase_uk.qm"
  Delete "$INSTDIR\uninst.exe"

  ; Remove directories used
  RMDir /r "$INSTDIR\translations"
  RMDir "$INSTDIR"
SectionEnd

Section "un.$(remove_shortcuts)" ;"un.Remove shortcuts"
  SectionIn RO
; Remove shortcuts, if any
  RMDir /r "$SMPROGRAMS\qBittorrent"
  Delete "$DESKTOP\qBittorrent.lnk"
SectionEnd

Section "un.$(remove_registry)" ;"un.Remove registry keys"
  SectionIn RO
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent"
  DeleteRegKey HKLM "Software\qBittorrent"
  ; Remove ProgIDs
  DeleteRegKey HKLM "Software\Classes\qBittorrent.File.Torrent"
  DeleteRegKey HKLM "Software\Classes\qBittorrent.Url.Magnet"
  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, i 0, i 0)'
SectionEnd

Section "un.$(remove_firewall)" ;

  DetailPrint $(remove_firewallinfo)
  nsisFirewallW::RemoveAuthorizedApplication "$INSTDIR\qbittorrent.exe"

SectionEnd

Section /o "un.$(remove_conf)" ;"un.Remove configuration files"

  !insertmacro UAC_AsUser_Call Function un.remove_conf_user ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}

SectionEnd

Function un.remove_conf_user

  System::Call 'shell32::SHGetSpecialFolderPath(i $HWNDPARENT, t .r1, i ${CSIDL_APPDATA}, i0)i.r0'
  RMDir /r "$1\qBittorrent"

FunctionEnd

Section /o "un.$(remove_cache)"

  !insertmacro UAC_AsUser_Call Function un.remove_cache_user ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}

SectionEnd

Function un.remove_cache_user

  System::Call 'shell32::SHGetSpecialFolderPath(i $HWNDPARENT, t .r1, i ${CSIDL_LOCALAPPDATA}, i0)i.r0'
  RMDir /r "$1\qBittorrent\"

FunctionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  !insertmacro Init "uninstaller"
  !insertmacro MUI_UNGETLANGUAGE

FunctionEnd

Function un.check_instance

    check:
    FindProcDLL::FindProc "qbittorrent.exe"
    StrCmp $R0 "1" 0 notfound
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION $(uninst_warning) IDRETRY check IDCANCEL done

    done:
    Abort

    notfound:

FunctionEnd

Function un.onUninstSuccess
  SetErrorLevel 0
FunctionEnd
