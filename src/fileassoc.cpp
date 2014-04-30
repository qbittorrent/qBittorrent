/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  glassez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : glassez@yandex.ru
 */

#include <QSysInfo>
#include <QMessageBox>
#include <QDebug>
#include <ShlObj.h>
#include <shobjidl.h>
#include "preferences.h"
#include "fileassoc.h"

namespace fileassoc
{

LPCWSTR pszAppRegistryName = L"qBittorrent";

void startupCheck_impl_v1(); // WinXP
#ifdef APP_ASSOC_REG
void startupCheck_impl_v2(); // Vista, Win7
void startupCheck_impl_v3(); // Win8
#else
void startupHint();
#endif

bool checkTorrent()
{
  qDebug() << Q_FUNC_INFO;
  QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
  if (settings.value(".torrent/Default").toString() != "qBittorrent.File.Torrent")
  {
    qDebug(".torrent != qBittorrent.File.Torrent");
    return false;
  }

  return true;
}

bool checkMagnet()
{
  qDebug() << Q_FUNC_INFO;
  QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
  if (settings.value("magnet/Default").toString() != "qBittorrent.Url.Magnet")
  {
    qDebug("magnet != qBittorrent.Url.Magnet");
    return false;
  }

  return true;
}

void setTorrent(bool set)
{
  qDebug() << Q_FUNC_INFO;
  QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

  // .Torrent association
  if (set)
    settings.setValue(".torrent/Default", "qBittorrent.File.Torrent");
  else
    settings.setValue(".torrent/Default", "");

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void setMagnet(bool set)
{
  qDebug() << Q_FUNC_INFO;
  QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

  // Magnet association
  if (set)
    settings.setValue("magnet/Default", "qBittorrent.Url.Magnet");
  else
    settings.setValue("magnet/Default", "");

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void startupCheck()
{
  qDebug() << Q_FUNC_INFO;
  if (Preferences().neverCheckFileAssoc()) return;

  if (QSysInfo::WindowsVersion < QSysInfo::WV_6_0)
    // WinXP (earlier versions is not supported by qBittorrent)
    startupCheck_impl_v1();
#ifdef APP_ASSOC_REG
  else if (QSysInfo::WindowsVersion <= QSysInfo::WV_6_1)
    // Vista, Win7
    startupCheck_impl_v2();
  else
    // 8, 8.1
    startupCheck_impl_v3();
#else
  else
    startupHint();
#endif
}

void startupCheck_impl_v1()
{
  qDebug() << Q_FUNC_INFO;
  const QString dlgTitle = QObject::tr("Torrent file association");
  const QString dlgText = QObject::tr(
        "qBittorrent is not the default application to open torrent files or Magnet links.\n"
        "Do you want to associate qBittorrent to torrent files and Magnet links?"
        );

  if (!checkTorrent() || !checkMagnet())
  {
    if (QMessageBox::question(0, dlgTitle, dlgText, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
    {
      setTorrent();
      setMagnet();
    }
    else
    {
      Preferences().setNeverCheckFileAssoc();
    }
  }
}

#ifdef APP_ASSOC_REG

void startupCheck_impl_v2()
{
  qDebug() << Q_FUNC_INFO;
  const QString dlgTitle = QObject::tr("Torrent file association");
  const QString dlgText = QObject::tr(
        "qBittorrent is not the default application to open torrent files or Magnet links.\n"
        "Do you want to associate qBittorrent to torrent files and Magnet links?"
        );

  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) return;

  IApplicationAssociationRegistration *pAssoc;

  hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 0, CLSCTX_INPROC,
                        __uuidof(IApplicationAssociationRegistration), reinterpret_cast<void**>(&pAssoc));
  if (SUCCEEDED(hr))
  {
    BOOL is_default = TRUE;

    hr = pAssoc->QueryAppIsDefaultAll(AL_EFFECTIVE, pszAppRegistryName, &is_default);
    if ((SUCCEEDED(hr) && (is_default == FALSE)) || (hr == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION)))
    {
      if (QMessageBox::question(0, dlgTitle, dlgText, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
      {
        pAssoc->SetAppAsDefaultAll(pszAppRegistryName);
      }
      else
      {
        Preferences().setNeverCheckFileAssoc();
      }
    }

    pAssoc->Release();
  }

  CoUninitialize();
}

void startupCheck_impl_v3()
{
  qDebug() << Q_FUNC_INFO;
  const QString dlgTitle = QObject::tr("Torrent file association");
  const QString dlgText = QObject::tr(
        "Do you want to set qBittorrent as default program for .torrent files\n"
        "and Magnet links?\n"
        "You can do it later using \"Default Programs\" dialog from Control Panel."
        );

  if (QMessageBox::question(0, dlgTitle, dlgText, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
  {
    showUI();
  }

  Preferences().setNeverCheckFileAssoc(true);
}

void showUI()
{
  qDebug() << Q_FUNC_INFO;
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) return;

  IApplicationAssociationRegistrationUI *pAssocUI;

  hr = CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI, 0, CLSCTX_INPROC,
                        __uuidof(IApplicationAssociationRegistrationUI), reinterpret_cast<void**>(&pAssocUI));
  if (SUCCEEDED(hr))
  {
    pAssocUI->LaunchAdvancedAssociationUI(pszAppRegistryName);
    pAssocUI->Release();
  }

  CoUninitialize();
}

#else

void startupHint()
{
  qDebug() << Q_FUNC_INFO;
  const QString dlgTitle = QObject::tr("Torrent file association");
  const QString dlgText = QObject::tr(
        "To set qBittorrent as default program for .torrent files and/or Magnet links\n"
        "you can use \"Default Programs\" dialog from Control Panel."
        );

  QMessageBox::information(0, dlgTitle, dlgText, QMessageBox::Ok);
  pref.setNeverCheckFileAssoc(true);
}

#endif

} // namespace fileassoc
