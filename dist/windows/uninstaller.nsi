Section "un.$(remove_files)" ;"un.Remove files"
  SectionIn RO

; Remove files and uninstaller
  Delete "$INSTDIR\qbittorrent.exe"
  Delete "$INSTDIR\qbittorrent.pdb"
  Delete "$INSTDIR\qt.conf"
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

Section "un.$(remove_associations)" ;"un.Remove file associations"
  SectionIn RO
  ReadRegStr $0 HKLM "Software\Classes\.torrent" ""
  StrCmp $0 "qBittorrent" 0 torrent_end
  DetailPrint "$(uninst_tor_warn) $0"
  DeleteRegValue HKLM "Software\Classes\.torrent" ""
  DeleteRegKey /ifempty HKLM "Software\Classes\.torrent"

  torrent_end:
  ReadRegStr $0 HKLM "Software\Classes\magnet\shell\open\command" ""
  StrCmp $0 '"$INSTDIR\qbittorrent.exe" "%1"' 0 magnet_end
  DetailPrint "$(uninst_mag_warn) $0"
  DeleteRegKey HKLM "Software\Classes\magnet"

  magnet_end:
  !insertmacro UAC_AsUser_Call Function un.remove_associations_user ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}

  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, i 0, i 0)'
SectionEnd

Function un.remove_associations_user
  ReadRegStr $0 HKCU "Software\Classes\.torrent" ""
  StrCmp $0 "qBittorrent" 0 torrent_end
  DetailPrint "$(uninst_tor_warn) $0"
  DeleteRegValue HKCU "Software\Classes\.torrent" ""
  DeleteRegKey /ifempty HKCU "Software\Classes\.torrent"

  torrent_end:
  ReadRegStr $0 HKCU "Software\Classes\magnet\shell\open\command" ""
  StrCmp $0 '"$INSTDIR\qbittorrent.exe" "%1"' 0 magnet_end
  DetailPrint "$(uninst_mag_warn) $0"
  DeleteRegKey HKCU "Software\Classes\magnet"

  magnet_end:
FunctionEnd

Section "un.$(remove_registry)" ;"un.Remove registry keys"
  SectionIn RO
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent"
  DeleteRegKey HKLM "Software\qBittorrent"
  DeleteRegKey HKLM "Software\Classes\qBittorrent"

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
