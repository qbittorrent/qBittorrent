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

Section "un.$(remove_registry)" ;"un.Remove registry keys"
  SectionIn RO
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qBittorrent"
  DeleteRegKey HKLM "Software\qBittorrent"
  ; Remove ProgIDs
  DeleteRegKey HKLM "Software\Classes\qBittorrent.File.Torrent"
  DeleteRegKey HKLM "Software\Classes\qBittorrent.Url.Magnet"
  System::Call 'Shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_IDLIST}, p 0, p 0)'
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
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION $(uninst_warning) /SD IDCANCEL IDRETRY check IDCANCEL canceled

  canceled:
  SetErrorLevel 15618 # WinError.h: `ERROR_PACKAGES_IN_USE`
  Abort

  notfound:

FunctionEnd

Function un.onUninstSuccess
  SetErrorLevel 0
FunctionEnd
