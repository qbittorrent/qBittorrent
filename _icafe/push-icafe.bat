@echo off
rem Tao nhanh icafe-nox-5.2 tu qBittorrent 5.2.4 + patch peer_id, roi push len fork de GitHub tu build.
setlocal
cd /d "%~dp0.."

git config user.name  >nul 2>&1 || git config user.name  "lienminhld49"
git config user.email >nul 2>&1 || git config user.email "net269g022@gmail.com"

echo [1/4] Lay tag release-5.2.4 tu upstream...
git fetch upstream tag release-5.2.4 --no-tags || goto :err

echo [2/4] Tao nhanh icafe-nox-5.2...
git switch -c icafe-nox-5.2 release-5.2.4 || goto :err

echo [3/4] Ap patch...
git am --keep-cr "_icafe\icafe-nox-5.2.patch" || goto :err

echo [4/4] Push len origin (lienminhld49/qBittorrent)...
git push -u origin icafe-nox-5.2 || goto :err

echo.
echo XONG. Vao https://github.com/lienminhld49/qBittorrent/actions de xem build.
pause
exit /b 0

:err
echo.
echo LOI o buoc tren. Chup man hinh gui lai cho Claude.
pause
exit /b 1
