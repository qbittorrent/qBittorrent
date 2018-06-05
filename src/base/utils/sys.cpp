/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#include "sys.h"

#ifdef Q_OS_WIN
#include <ShlObj.h>
#include <shobjidl.h>

#include <QCoreApplication>
#include <QMessageBox>
#include <QSettings>
#include <QSysInfo>

#include "base/preferences.h"

namespace
{
    LPCWSTR APP_REGISTRY_NAME = L"qBittorrent";

    inline QString tr(const char *message)
    {
        return qApp->translate("System", message);
    }

    void showInitialAssocUIImpl1()
    {
        using namespace Utils::Sys;

        const QString dlgTitle {tr("Torrent file association")};
        const QString dlgText {
            tr("qBittorrent is not the default application to open torrent files or Magnet links.\n"
               "Do you want to associate qBittorrent to torrent files and Magnet links?")
        };

        if (!isTorrentFileAssocSet() || !isMagnetLinkAssocSet())
        {
            if (QMessageBox::question(nullptr, dlgTitle, dlgText, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
                setTorrentFileAssoc();
                setMagnetLinkAssoc();
            }
            else {
                Preferences::instance()->setNeverCheckFileAssoc();
            }
        }
    }

#ifdef APP_ASSOC_REG
    void showInitialAssocUIImpl2()
    {
        const QString dlgTitle {tr("Torrent file association")};
        const QString dlgText {
            tr("qBittorrent is not the default application to open torrent files or Magnet links.\n"
               "Do you want to associate qBittorrent to torrent files and Magnet links?")
        };

        HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr)) return;

        IApplicationAssociationRegistration *pAssoc;

        hr = ::CoCreateInstance(
                 CLSID_ApplicationAssociationRegistration, 0, CLSCTX_INPROC
                 ,__uuidof(IApplicationAssociationRegistration), reinterpret_cast<void**>(&pAssoc));
        if (SUCCEEDED(hr)) {
            BOOL isDefault = TRUE;

            hr = pAssoc->QueryAppIsDefaultAll(AL_EFFECTIVE, APP_REGISTRY_NAME, &isDefault);
            if ((SUCCEEDED(hr) && (isDefault == FALSE)) || (hr == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION))) {
                if (QMessageBox::question(nullptr, dlgTitle, dlgText, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
                    pAssoc->SetAppAsDefaultAll(APP_REGISTRY_NAME);
                else
                    Preferences::instance()->setNeverCheckFileAssoc();
            }

            pAssoc->Release();
        }

        ::CoUninitialize();
    }

    void showInitialAssocUIImpl3()
    {
        const QString dlgTitle {tr("Torrent file association")};
        const QString dlgText {
            tr("Do you want to set qBittorrent as default program for .torrent files\n"
               "and Magnet links?\n"
               "You can do it later using \"Default Programs\" dialog from Control Panel.")
        };

        if (QMessageBox::question(nullptr, dlgTitle, dlgText, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
        {
            Utils::Sys::showAppAssocRegUI();
        }

        Preferences::instance()->setNeverCheckFileAssoc();
    }
#else
    void showInitialAssocUIImplFallback()
    {
        const QString dlgTitle {tr("Torrent file association")};
        const QString dlgText {
            tr("To set qBittorrent as default program for .torrent files and/or Magnet links\n"
               "you can use \"Default Programs\" dialog from Control Panel.")
        };

        QMessageBox::information(nullptr, dlgTitle, dlgText, QMessageBox::Ok);
        Preferences::instance()->setNeverCheckFileAssoc();
    }
#endif
}

bool Utils::Sys::isTorrentFileAssocSet()
{
    QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
    if (settings.value(".torrent/Default").toString() != "qBittorrent.File.Torrent")
        return false;

    return true;
}

bool Utils::Sys::isMagnetLinkAssocSet()
{
    QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
    if (settings.value("magnet/Default").toString() != "qBittorrent.Url.Magnet")
        return false;

    return true;
}

void Utils::Sys::setTorrentFileAssoc(bool value)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // .Torrent association
    if (value)
        settings.setValue(".torrent/Default", "qBittorrent.File.Torrent");
    else
        settings.setValue(".torrent/Default", "");

    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void Utils::Sys::setMagnetLinkAssoc(bool value)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Magnet association
    if (value)
        settings.setValue("magnet/Default", "qBittorrent.Url.Magnet");
    else
        settings.setValue("magnet/Default", "");

    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void Utils::Sys::showAppAssocRegUI()
{
    HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return;

    IApplicationAssociationRegistrationUI *pAssocUI;

    hr = ::CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI
                            , 0, CLSCTX_INPROC
                            , __uuidof(IApplicationAssociationRegistrationUI)
                            , reinterpret_cast<void**>(&pAssocUI));
    if (SUCCEEDED(hr)) {
        pAssocUI->LaunchAdvancedAssociationUI(APP_REGISTRY_NAME);
        pAssocUI->Release();
    }

    ::CoUninitialize();
}

void Utils::Sys::showInitialAssocUI()
{
    if (QSysInfo::WindowsVersion < QSysInfo::WV_6_0)
        // WinXP (earlier versions is not supported by qBittorrent)
        showInitialAssocUIImpl1();
#ifdef APP_ASSOC_REG
    else if (QSysInfo::WindowsVersion <= QSysInfo::WV_6_1)
        // Vista, Win7
        showInitialAssocUIImpl2();
    else
        // 8, 8.1
        showInitialAssocUIImpl3();
#else
    else
        showInitialAssocUIImplFallback();
#endif
}
#endif
