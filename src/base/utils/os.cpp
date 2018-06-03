/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2014  sledgehammer999 <sledgehammer999@qbittorrent.org>
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

#include "os.h"

#ifdef Q_OS_WIN
#include <stdexcept>

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <boost/intrusive_ptr.hpp>

#include <QDir>
#include <QSettings>
#include <QSysInfo>

#include <base/global.h>

#define __WIDE(str) L ## str
#define WIDE(str) __WIDE(str)

#define APP_REGISTRY_NAME WIDE(QBT_APP_NAME)

namespace
{
    const QString CLASS_TORRENT {QLatin1String(C_TORRENT_FILE_EXTENSION)};
    const QString CLASS_MAGNET {QStringLiteral("magnet")};
    const QString PROGID_TORRENT {QStringLiteral("qBittorrent.File.Torrent")};
    const QString PROGID_MAGNET {QStringLiteral("qBittorrent.Url.Magnet")};

    void intrusive_ptr_add_ref(IUnknown *pCOMObj)
    {
        pCOMObj->AddRef();
    }

    void intrusive_ptr_release(IUnknown *pCOMObj)
    {
        pCOMObj->Release();
    }

    class COMLib final
    {
        Q_DISABLE_COPY(COMLib)

    public:
        COMLib(DWORD dwCoInit)
        {
            if (FAILED(::CoInitializeEx(NULL, dwCoInit)))
                throw std::runtime_error("Couldn't initialize COM library.");
        }

        ~COMLib()
        {
            ::CoUninitialize();
        }

        template <typename T>
        boost::intrusive_ptr<T> createObject(
                REFCLSID rclsid, LPUNKNOWN pUnkOuter
                , DWORD dwClsContext, HRESULT *pError = nullptr) const
        {
            T *obj = nullptr;
            HRESULT result = ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T)
                                                , reinterpret_cast<void**>(&obj));
            if (pError)
                *pError = result;

            return {obj, false};
        }
    };

    enum REG_SEARCH_TYPE
    {
        USER,
        SYSTEM_32BIT,
        SYSTEM_64BIT
    };

    QStringList getRegSubkeys(HKEY handle)
    {
        QStringList keys;

        DWORD cSubKeys = 0;
        DWORD cMaxSubKeyLen = 0;
        LONG res = ::RegQueryInfoKeyW(handle, NULL, NULL, NULL, &cSubKeys, &cMaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);

        if (res == ERROR_SUCCESS) {
            ++cMaxSubKeyLen; // For null character
            LPWSTR lpName = new WCHAR[cMaxSubKeyLen];
            DWORD cName;

            for (DWORD i = 0; i < cSubKeys; ++i) {
                cName = cMaxSubKeyLen;
                res = ::RegEnumKeyExW(handle, i, lpName, &cName, NULL, NULL, NULL, NULL);
                if (res == ERROR_SUCCESS)
                    keys.push_back(QString::fromWCharArray(lpName));
            }

            delete[] lpName;
        }

        return keys;
    }

    QString getRegValue(HKEY handle, const QString &name = QString())
    {
        QString result;

        DWORD type = 0;
        DWORD cbData = 0;
        LPWSTR lpValueName = NULL;
        if (!name.isEmpty()) {
            lpValueName = new WCHAR[name.size() + 1];
            name.toWCharArray(lpValueName);
            lpValueName[name.size()] = 0;
        }

        // Discover the size of the value
        ::RegQueryValueExW(handle, lpValueName, NULL, &type, NULL, &cbData);
        DWORD cBuffer = (cbData / sizeof(WCHAR)) + 1;
        LPWSTR lpData = new WCHAR[cBuffer];
        LONG res = ::RegQueryValueExW(handle, lpValueName, NULL, &type, (LPBYTE)lpData, &cbData);
        if (lpValueName)
            delete[] lpValueName;

        if (res == ERROR_SUCCESS) {
            lpData[cBuffer - 1] = 0;
            result = QString::fromWCharArray(lpData);
        }
        delete[] lpData;

        return result;
    }

    QString pythonSearchReg(const REG_SEARCH_TYPE type)
    {
        HKEY hkRoot;
        if (type == USER)
            hkRoot = HKEY_CURRENT_USER;
        else
            hkRoot = HKEY_LOCAL_MACHINE;

        REGSAM samDesired = KEY_READ;
        if (type == SYSTEM_32BIT)
            samDesired |= KEY_WOW64_32KEY;
        else if (type == SYSTEM_64BIT)
            samDesired |= KEY_WOW64_64KEY;

        QString path;
        LONG res = 0;
        HKEY hkPythonCore;
        res = ::RegOpenKeyExW(hkRoot, L"SOFTWARE\\Python\\PythonCore", 0, samDesired, &hkPythonCore);

        if (res == ERROR_SUCCESS) {
            QStringList versions = getRegSubkeys(hkPythonCore);
            qDebug("Python versions nb: %d", versions.size());
            versions.sort();

            bool found = false;
            while (!found && !versions.empty()) {
                const QString version = versions.takeLast() + "\\InstallPath";
                LPWSTR lpSubkey = new WCHAR[version.size() + 1];
                version.toWCharArray(lpSubkey);
                lpSubkey[version.size()] = 0;

                HKEY hkInstallPath;
                res = ::RegOpenKeyExW(hkPythonCore, lpSubkey, 0, samDesired, &hkInstallPath);
                delete[] lpSubkey;

                if (res == ERROR_SUCCESS) {
                    qDebug("Detected possible Python v%s location", qUtf8Printable(version));
                    path = getRegValue(hkInstallPath);
                    ::RegCloseKey(hkInstallPath);

                    if (!path.isEmpty() && QDir(path).exists("python.exe")) {
                        qDebug("Found python.exe at %s", qUtf8Printable(path));
                        found = true;
                    }
                }
            }

            if (!found)
                path = QString();

            ::RegCloseKey(hkPythonCore);
        }

        return path;
    }

    QString getAssoc(const QString &cls)
    {
        return (QSettings {"HKEY_CLASSES_ROOT", QSettings::NativeFormat}).value(cls + "/Default").toString();
    }

    void setAssoc(const QString &cls, const QString &progID)
    {
        QSettings reg {"HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat};
        const QString oldProgID = reg.value(cls + "/Default").toString();
        if (!oldProgID.isEmpty() && (oldProgID != progID))
            reg.setValue(cls + "/OpenWithProgids/" + oldProgID, "");
        reg.setValue(cls + "/Default", progID);
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
    }
}

QString Utils::OS::getPythonPath()
{
    QString path = pythonSearchReg(USER);
    if (!path.isEmpty())
        return path;

    path = pythonSearchReg(SYSTEM_32BIT);
    if (!path.isEmpty())
        return path;

    path = pythonSearchReg(SYSTEM_64BIT);
    if (!path.isEmpty())
        return path;

    // Fallback: Detect python from default locations
    const QStringList dirs = QDir("C:/").entryList(QStringList("Python*"), QDir::Dirs, QDir::Name | QDir::Reversed);
    foreach (const QString &dir, dirs) {
        const QString path("C:/" + dir + "/");
        if (QFile::exists(path + "python.exe"))
            return path;
    }

    return QString();
}

bool Utils::OS::areAssociationsSet() noexcept
{
#ifndef WIN_HAS_DEFAULT_PROGRAMS_API
    Q_ASSERT(QSysInfo::WindowsVersion < QSysInfo::WV_VISTA);
    return (isTorrentFileAssocSet() && isMagnetLinkAssocSet());
#else
    Q_ASSERT(QSysInfo::WindowsVersion < QSysInfo::WV_WINDOWS8);

    if (QSysInfo::WindowsVersion < QSysInfo::WV_VISTA)
        return (isTorrentFileAssocSet() && isMagnetLinkAssocSet());

    // Vista, Win7
    BOOL isDefault = FALSE;
    try {
        const COMLib comLib {COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE};


        auto pAssoc = comLib.createObject<IApplicationAssociationRegistration>(
                          CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC);
        if (pAssoc)
            pAssoc->QueryAppIsDefaultAll(AL_EFFECTIVE, APP_REGISTRY_NAME, &isDefault);
    }
    catch (const std::runtime_error &) {}

    return (isDefault == TRUE);
#endif
}

void Utils::OS::setAssociations()
{
#ifndef WIN_HAS_DEFAULT_PROGRAMS_API
    Q_ASSERT(QSysInfo::WindowsVersion < QSysInfo::WV_VISTA);

    setTorrentFileAssoc(true);
    setMagnetLinkAssoc(true);
#else
    Q_ASSERT(QSysInfo::WindowsVersion < QSysInfo::WV_WINDOWS8);

    if (QSysInfo::WindowsVersion >= QSysInfo::WV_VISTA) {
        // Vista, Win7
        const COMLib comLib {COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE};

        auto pAssoc = comLib.createObject<IApplicationAssociationRegistration>(
                           CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC);
        if (!pAssoc || FAILED(pAssoc->SetAppAsDefaultAll(APP_REGISTRY_NAME)))
            throw std::runtime_error("An error occurred while set application associations.");
    }
    else {
        setTorrentFileAssoc(true);
        setMagnetLinkAssoc(true);
    }
#endif
}

bool Utils::OS::isTorrentFileAssocSet()
{
    if (QSysInfo::WindowsVersion >= QSysInfo::WV_VISTA)
        return false;

    return (getAssoc(CLASS_TORRENT) == PROGID_TORRENT);
}

bool Utils::OS::isMagnetLinkAssocSet()
{
    if (QSysInfo::WindowsVersion >= QSysInfo::WV_VISTA)
        return false;

    return (getAssoc(CLASS_MAGNET) == PROGID_MAGNET);
}

void Utils::OS::setTorrentFileAssoc(bool value)
{
    Q_ASSERT(QSysInfo::WindowsVersion < QSysInfo::WV_VISTA);

    if (value)
        setAssoc(CLASS_TORRENT, PROGID_TORRENT);
    else if (isTorrentFileAssocSet())
        setAssoc(CLASS_TORRENT, "");
}

void Utils::OS::setMagnetLinkAssoc(bool value)
{
    Q_ASSERT(QSysInfo::WindowsVersion < QSysInfo::WV_VISTA);

    if (value)
        setAssoc(CLASS_MAGNET, "qBittorrent.Url.Magnet");
    else if (isMagnetLinkAssocSet())
        setAssoc(CLASS_MAGNET, PROGID_MAGNET);
}

#ifdef WIN_HAS_DEFAULT_PROGRAMS_API
void Utils::OS::showAssociationRegistrationUI ()
{
    const COMLib comLib {COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE};

    auto pAssocUI = comLib.createObject<IApplicationAssociationRegistrationUI>(
                   CLSID_ApplicationAssociationRegistrationUI, 0, CLSCTX_INPROC);
    if (!pAssocUI || FAILED(pAssocUI->LaunchAdvancedAssociationUI(APP_REGISTRY_NAME)))
        throw std::runtime_error("An error occurred while show \"Default Programs\" dialog.");
}
#endif
#endif

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>

namespace
{
    CFStringRef torrentExtension = CFSTR("torrent");
    CFStringRef magnetUrlScheme = CFSTR("magnet");
}

bool Utils::OS::isTorrentFileAssocSet()
{
    bool isSet = false;
    CFStringRef torrentId = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL) {
        CFStringRef defaultHandlerId = LSCopyDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer);
        if (defaultHandlerId != NULL) {
            CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
            isSet = CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
            CFRelease(defaultHandlerId);
        }
        CFRelease(torrentId);
    }
    return isSet;
}

bool Utils::OS::isMagnetLinkAssocSet()
{
    bool isSet = false;
    CFStringRef defaultHandlerId = LSCopyDefaultHandlerForURLScheme(magnetUrlScheme);
    if (defaultHandlerId != NULL) {
        CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
        isSet = CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
        CFRelease(defaultHandlerId);
    }
    return isSet;
}

void Utils::OS::setTorrentFileAssoc()
{
    if (isTorrentFileAssocSet())
        return;
    CFStringRef torrentId = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL) {
        CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
        LSSetDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer, myBundleId);
        CFRelease(torrentId);
    }
}

void Utils::OS::setMagnetLinkAssoc()
{
    if (isMagnetLinkAssocSet())
        return;
    CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
    LSSetDefaultHandlerForURLScheme(magnetUrlScheme, myBundleId);
}
#endif
