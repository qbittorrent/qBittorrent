/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Mike Tzou (Chocobo1)
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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
#include <algorithm>

#include <windows.h>
#include <powrprof.h>
#include <shlobj.h>
#endif // Q_OS_WIN

#ifdef Q_OS_MACOS
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif // Q_OS_MACOS

#include <QScopeGuard>

#ifdef QBT_USES_DBUS
#include <QDBusInterface>
#endif // QBT_USES_DBUS

#ifdef Q_OS_WIN
#include <QCoreApplication>
#endif // Q_OS_WIN

#include "base/global.h"
#include "base/types.h"

void Utils::OS::shutdownComputer([[maybe_unused]] const ShutdownDialogAction &action)
{
#if defined(Q_OS_WIN)
    HANDLE hToken;            // handle to process token
    TOKEN_PRIVILEGES tkp;     // pointer to token structure
    if (!::OpenProcessToken(::GetCurrentProcess(), (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken))
        return;
    // Get the LUID for shutdown privilege.
    ::LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1; // one privilege to set
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Get shutdown privilege for this process.

    ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

    // Cannot test the return value of AdjustTokenPrivileges.

    if (::GetLastError() != ERROR_SUCCESS)
        return;

    if (action == ShutdownDialogAction::Suspend)
    {
        ::SetSuspendState(FALSE, FALSE, FALSE);
    }
    else if (action == ShutdownDialogAction::Hibernate)
    {
        ::SetSuspendState(TRUE, FALSE, FALSE);
    }
    else
    {
        std::wstring msg = QCoreApplication::translate("misc"
            , "qBittorrent will shutdown the computer now because all downloads are complete.").toStdWString();
        ::InitiateSystemShutdownW(nullptr, msg.data(), 10, TRUE, FALSE);
    }

    // Disable shutdown privilege.
    tkp.Privileges[0].Attributes = 0;
    ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

#elif defined(Q_OS_MACOS)
    AEEventID EventToSend;
    if (action != ShutdownDialogAction::Shutdown)
        EventToSend = kAESleep;
    else
        EventToSend = kAEShutDown;
    AEAddressDesc targetDesc;
    const ProcessSerialNumber kPSNOfSystemProcess = {0, kSystemProcess};
    AppleEvent eventReply = {typeNull, NULL};
    AppleEvent appleEventToSend = {typeNull, NULL};

    OSStatus error = ::AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess
        , sizeof(kPSNOfSystemProcess), &targetDesc);

    if (error != noErr)
        return;

    error = ::AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc, kAutoGenerateReturnID
        , kAnyTransactionID, &appleEventToSend);

    AEDisposeDesc(&targetDesc);
    if (error != noErr)
        return;

    error = ::AESend(&appleEventToSend, &eventReply, kAENoReply, kAENormalPriority, kAEDefaultTimeout
        , NULL, NULL);

    ::AEDisposeDesc(&appleEventToSend);
    if (error != noErr)
        return;

    ::AEDisposeDesc(&eventReply);

#elif defined(QBT_USES_DBUS)
    // Use dbus to power off / suspend the system
    if (action != ShutdownDialogAction::Shutdown)
    {
        // Some recent systems use systemd's logind
        QDBusInterface login1Iface(u"org.freedesktop.login1"_s, u"/org/freedesktop/login1"_s,
                                   u"org.freedesktop.login1.Manager"_s, QDBusConnection::systemBus());
        if (login1Iface.isValid())
        {
            if (action == ShutdownDialogAction::Suspend)
                login1Iface.call(u"Suspend"_s, false);
            else
                login1Iface.call(u"Hibernate"_s, false);
            return;
        }
        // Else, other recent systems use UPower
        QDBusInterface upowerIface(u"org.freedesktop.UPower"_s, u"/org/freedesktop/UPower"_s,
                                   u"org.freedesktop.UPower"_s, QDBusConnection::systemBus());
        if (upowerIface.isValid())
        {
            if (action == ShutdownDialogAction::Suspend)
                upowerIface.call(u"Suspend"_s);
            else
                upowerIface.call(u"Hibernate"_s);
            return;
        }
        // HAL (older systems)
        QDBusInterface halIface(u"org.freedesktop.Hal"_s, u"/org/freedesktop/Hal/devices/computer"_s,
                                u"org.freedesktop.Hal.Device.SystemPowerManagement"_s,
                                QDBusConnection::systemBus());
        if (action == ShutdownDialogAction::Suspend)
            halIface.call(u"Suspend"_s, 5);
        else
            halIface.call(u"Hibernate"_s);
    }
    else
    {
        // Some recent systems use systemd's logind
        QDBusInterface login1Iface(u"org.freedesktop.login1"_s, u"/org/freedesktop/login1"_s,
                                   u"org.freedesktop.login1.Manager"_s, QDBusConnection::systemBus());
        if (login1Iface.isValid())
        {
            login1Iface.call(u"PowerOff"_s, false);
            return;
        }
        // Else, other recent systems use ConsoleKit
        QDBusInterface consolekitIface(u"org.freedesktop.ConsoleKit"_s, u"/org/freedesktop/ConsoleKit/Manager"_s,
                                       u"org.freedesktop.ConsoleKit.Manager"_s, QDBusConnection::systemBus());
        if (consolekitIface.isValid())
        {
            consolekitIface.call(u"Stop"_s);
            return;
        }
        // HAL (older systems)
        QDBusInterface halIface(u"org.freedesktop.Hal"_s, u"/org/freedesktop/Hal/devices/computer"_s,
                                u"org.freedesktop.Hal.Device.SystemPowerManagement"_s,
                                QDBusConnection::systemBus());
        halIface.call(u"Shutdown"_s);
    }
#endif
}

#ifdef Q_OS_MACOS
namespace
{
    const CFStringRef torrentExtension = CFSTR("torrent");
    const CFStringRef magnetUrlScheme = CFSTR("magnet");
}

bool Utils::OS::isTorrentFileAssocSet()
{
    bool isSet = false;
    const CFStringRef torrentId = ::UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL)
    {
        const CFStringRef defaultHandlerId = ::LSCopyDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer);
        if (defaultHandlerId != NULL)
        {
            const CFStringRef myBundleId = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
            if (myBundleId != NULL)
                isSet = ::CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
            ::CFRelease(defaultHandlerId);
        }
        ::CFRelease(torrentId);
    }
    return isSet;
}

void Utils::OS::setTorrentFileAssoc()
{
    if (isTorrentFileAssocSet())
        return;

    const CFStringRef torrentId = ::UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL)
    {
        const CFStringRef myBundleId = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
        if (myBundleId != NULL)
            ::LSSetDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer, myBundleId);
        ::CFRelease(torrentId);
    }
}

bool Utils::OS::isMagnetLinkAssocSet()
{
    bool isSet = false;
    const CFStringRef defaultHandlerId = ::LSCopyDefaultHandlerForURLScheme(magnetUrlScheme);
    if (defaultHandlerId != NULL)
    {
        const CFStringRef myBundleId = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
        if (myBundleId != NULL)
            isSet = ::CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
        ::CFRelease(defaultHandlerId);
    }
    return isSet;
}

void Utils::OS::setMagnetLinkAssoc()
{
    if (isMagnetLinkAssocSet())
        return;

    const CFStringRef myBundleId = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
    if (myBundleId != NULL)
        ::LSSetDefaultHandlerForURLScheme(magnetUrlScheme, myBundleId);
}
#endif // Q_OS_MACOS

#ifdef Q_OS_WIN
Path Utils::OS::windowsSystemPath()
{
    static const Path path = []() -> Path
    {
        WCHAR systemPath[MAX_PATH] = {0};
        ::GetSystemDirectoryW(systemPath, sizeof(systemPath) / sizeof(WCHAR));
        return Path(QString::fromWCharArray(systemPath));
    }();
    return path;
}
#endif // Q_OS_WIN

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
bool Utils::OS::applyMarkOfTheWeb(const Path &file, const QString &url)
{
    // Trying to apply this to a non-existent file is unacceptable,
    // as it may unexpectedly create such a file.
    if (!file.exists())
        return false;

    Q_ASSERT(url.isEmpty() || url.startsWith(u"http:") || url.startsWith(u"https:"));

#ifdef Q_OS_MACOS
    // References:
    // https://searchfox.org/mozilla-central/rev/ffdc4971dc18e1141cb2a90c2b0b776365650270/xpcom/io/CocoaFileUtils.mm#230
    // https://github.com/transmission/transmission/blob/f62f7427edb1fd5c430e0ef6956bbaa4f03ae597/macosx/Torrent.mm#L1945-L1955

    const CFStringRef fileString = file.toString().toCFString();
    [[maybe_unused]] const auto fileStringGuard = qScopeGuard([&fileString] { ::CFRelease(fileString); });
    const CFURLRef fileURL = ::CFURLCreateWithFileSystemPath(kCFAllocatorDefault
        , fileString, kCFURLPOSIXPathStyle, false);
    [[maybe_unused]] const auto fileURLGuard = qScopeGuard([&fileURL] { ::CFRelease(fileURL); });

    if (CFDictionaryRef currentProperties = nullptr;
        ::CFURLCopyResourcePropertyForKey(fileURL, kCFURLQuarantinePropertiesKey, &currentProperties, NULL)
            && currentProperties)
    {
        [[maybe_unused]] const auto currentPropertiesGuard = qScopeGuard([&currentProperties] { ::CFRelease(currentProperties); });

        if (CFStringRef quarantineType = nullptr;
            ::CFDictionaryGetValueIfPresent(currentProperties, kLSQuarantineTypeKey, reinterpret_cast<const void **>(&quarantineType))
                && quarantineType)
        {
            if (::CFStringCompare(quarantineType, kLSQuarantineTypeOtherDownload, 0) == kCFCompareEqualTo)
                return true;
        }
    }

    CFMutableDictionaryRef properties = ::CFDictionaryCreateMutable(kCFAllocatorDefault, 0
        , &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!properties)
        return false;
    [[maybe_unused]] const auto propertiesGuard = qScopeGuard([&properties] { ::CFRelease(properties); });

    ::CFDictionarySetValue(properties, kLSQuarantineTypeKey, kLSQuarantineTypeOtherDownload);
    if (!url.isEmpty())
    {
        const CFStringRef urlCFString = url.toCFString();
        [[maybe_unused]] const auto urlStringGuard = qScopeGuard([&urlCFString] { ::CFRelease(urlCFString); });
        ::CFDictionarySetValue(properties, kLSQuarantineDataURLKey, urlCFString);
    }

    const Boolean success = ::CFURLSetResourcePropertyForKey(fileURL, kCFURLQuarantinePropertiesKey
        , properties, NULL);
    return success;
#elif defined(Q_OS_WIN)
    const QString zoneIDStream = file.toString() + u":Zone.Identifier";

    HANDLE handle = ::CreateFileW(zoneIDStream.toStdWString().c_str(), (GENERIC_READ | GENERIC_WRITE)
        , (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE)
        , nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    [[maybe_unused]] const auto handleGuard = qScopeGuard([&handle] { ::CloseHandle(handle); });

    // 5.6.1 Zone.Identifier Stream Name
    // https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/6e3f7352-d11c-4d76-8c39-2516a9df36e8
    const QString hostURL = !url.isEmpty() ? url : u"about:internet"_s;
    const QByteArray zoneID = QByteArrayLiteral("[ZoneTransfer]\r\nZoneId=3\r\n")
        + u"HostUrl=%1\r\n"_s.arg(hostURL).toUtf8();

    if (LARGE_INTEGER streamSize = {0};
        ::GetFileSizeEx(handle, &streamSize) && (streamSize.QuadPart > 0))
    {
        const DWORD expectedReadSize = std::min<LONGLONG>(streamSize.QuadPart, 1024);
        QByteArray buf {expectedReadSize, '\0'};

        if (DWORD actualReadSize = 0;
            ::ReadFile(handle, buf.data(), expectedReadSize, &actualReadSize, nullptr) && (actualReadSize == expectedReadSize))
        {
            if (buf.startsWith("[ZoneTransfer]\r\n") && buf.contains("\r\nZoneId=3\r\n") && buf.contains("\r\nHostUrl="))
                return true;
        }
    }

    if (!::SetFilePointerEx(handle, {0}, nullptr, FILE_BEGIN))
        return false;
    if (!::SetEndOfFile(handle))
        return false;

    DWORD written = 0;
    const BOOL writeResult = ::WriteFile(handle, zoneID.constData(), zoneID.size(), &written, nullptr);
    return writeResult && (written == zoneID.size());
#endif
}
#endif // Q_OS_MACOS || Q_OS_WIN
