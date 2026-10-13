@echo off
rem qbittorrent-nox (custom build)
rem Mac dinh dung profile hien tai cua qBittorrent:
rem   %APPDATA%\qBittorrent\qBittorrent.ini  va  %LOCALAPPDATA%\qBittorrent\BT_backup
rem KHONG chay cung luc voi qBittorrent ban GUI tren cung profile.
rem Chay thu tren ban sao profile:  run-nox.bat --profile="D:\qbt-test-profile" --webui-port=8081
cd /d "%~dp0"
qbittorrent-nox.exe --confirm-legal-notice %*
