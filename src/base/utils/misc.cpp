/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "misc.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <powrprof.h>
#include <Shlobj.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef Q_OS_MACOS
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#endif

#include <boost/version.hpp>
#include <libtorrent/version.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <zlib.h>

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>
#include <QSysInfo>
#include <QVector>

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
#include <QDBusInterface>
#endif

#include "base/types.h"
#include "base/unicodestrings.h"
#include "base/utils/string.h"

namespace
{
    const struct { const char *source; const char *comment; } units[] =
    {
        QT_TRANSLATE_NOOP3("misc", "B", "bytes"),
        QT_TRANSLATE_NOOP3("misc", "KiB", "kibibytes (1024 bytes)"),
        QT_TRANSLATE_NOOP3("misc", "MiB", "mebibytes (1024 kibibytes)"),
        QT_TRANSLATE_NOOP3("misc", "GiB", "gibibytes (1024 mibibytes)"),
        QT_TRANSLATE_NOOP3("misc", "TiB", "tebibytes (1024 gibibytes)"),
        QT_TRANSLATE_NOOP3("misc", "PiB", "pebibytes (1024 tebibytes)"),
        QT_TRANSLATE_NOOP3("misc", "EiB", "exbibytes (1024 pebibytes)")
    };

    // return best userfriendly storage unit (B, KiB, MiB, GiB, TiB, ...)
    // use Binary prefix standards from IEC 60027-2
    // see http://en.wikipedia.org/wiki/Kilobyte
    // value must be given in bytes
    // to send numbers instead of strings with suffixes
    bool splitToFriendlyUnit(const qint64 sizeInBytes, qreal &val, Utils::Misc::SizeUnit &unit)
    {
        if (sizeInBytes < 0) return false;

        int i = 0;
        val = static_cast<qreal>(sizeInBytes);

        while ((val >= 1024.) && (i <= static_cast<int>(Utils::Misc::SizeUnit::ExbiByte)))
        {
            val /= 1024.;
            ++i;
        }

        unit = static_cast<Utils::Misc::SizeUnit>(i);
        return true;
    }
}

void Utils::Misc::shutdownComputer(const ShutdownDialogAction &action)
{
#if defined(Q_OS_WIN)
    HANDLE hToken;            // handle to process token
    TOKEN_PRIVILEGES tkp;     // pointer to token structure
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return;
    // Get the LUID for shutdown privilege.
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
                         &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1; // one privilege to set
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Get shutdown privilege for this process.

    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                          (PTOKEN_PRIVILEGES) NULL, 0);

    // Cannot test the return value of AdjustTokenPrivileges.

    if (GetLastError() != ERROR_SUCCESS)
        return;

    if (action == ShutdownDialogAction::Suspend)
    {
        ::SetSuspendState(false, false, false);
    }
    else if (action == ShutdownDialogAction::Hibernate)
    {
        ::SetSuspendState(true, false, false);
    }
    else
    {
        const QString msg = QCoreApplication::translate("misc", "qBittorrent will shutdown the computer now because all downloads are complete.");
        auto msgWchar = std::make_unique<wchar_t[]>(msg.length() + 1);
        msg.toWCharArray(msgWchar.get());
        ::InitiateSystemShutdownW(nullptr, msgWchar.get(), 10, true, false);
    }

    // Disable shutdown privilege.
    tkp.Privileges[0].Attributes = 0;
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES) NULL, 0);

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

    OSStatus error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                                  sizeof(kPSNOfSystemProcess), &targetDesc);

    if (error != noErr)
        return;

    error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                               kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);

    AEDisposeDesc(&targetDesc);
    if (error != noErr)
        return;

    error = AESend(&appleEventToSend, &eventReply, kAENoReply,
                   kAENormalPriority, kAEDefaultTimeout, NULL, NULL);

    AEDisposeDesc(&appleEventToSend);
    if (error != noErr)
        return;

    AEDisposeDesc(&eventReply);

#elif (defined(Q_OS_UNIX) && defined(QT_DBUS_LIB))
    // Use dbus to power off / suspend the system
    if (action != ShutdownDialogAction::Shutdown)
    {
        // Some recent systems use systemd's logind
        QDBusInterface login1Iface("org.freedesktop.login1", "/org/freedesktop/login1",
                                   "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
        if (login1Iface.isValid())
        {
            if (action == ShutdownDialogAction::Suspend)
                login1Iface.call("Suspend", false);
            else
                login1Iface.call("Hibernate", false);
            return;
        }
        // Else, other recent systems use UPower
        QDBusInterface upowerIface("org.freedesktop.UPower", "/org/freedesktop/UPower",
                                   "org.freedesktop.UPower", QDBusConnection::systemBus());
        if (upowerIface.isValid())
        {
            if (action == ShutdownDialogAction::Suspend)
                upowerIface.call("Suspend");
            else
                upowerIface.call("Hibernate");
            return;
        }
        // HAL (older systems)
        QDBusInterface halIface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                                "org.freedesktop.Hal.Device.SystemPowerManagement",
                                QDBusConnection::systemBus());
        if (action == ShutdownDialogAction::Suspend)
            halIface.call("Suspend", 5);
        else
            halIface.call("Hibernate");
    }
    else
    {
        // Some recent systems use systemd's logind
        QDBusInterface login1Iface("org.freedesktop.login1", "/org/freedesktop/login1",
                                   "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
        if (login1Iface.isValid())
        {
            login1Iface.call("PowerOff", false);
            return;
        }
        // Else, other recent systems use ConsoleKit
        QDBusInterface consolekitIface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager",
                                       "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());
        if (consolekitIface.isValid())
        {
            consolekitIface.call("Stop");
            return;
        }
        // HAL (older systems)
        QDBusInterface halIface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                                "org.freedesktop.Hal.Device.SystemPowerManagement",
                                QDBusConnection::systemBus());
        halIface.call("Shutdown");
    }

#else
    Q_UNUSED(action);
#endif
}

QString Utils::Misc::unitString(const SizeUnit unit, const bool isSpeed)
{
    const auto &unitString = units[static_cast<int>(unit)];
    QString ret = QCoreApplication::translate("misc", unitString.source, unitString.comment);
    if (isSpeed)
        ret += QCoreApplication::translate("misc", "/s", "per second");
    return ret;
}

QString Utils::Misc::friendlyUnit(const qint64 bytesValue, const bool isSpeed)
{
    SizeUnit unit;
    qreal friendlyVal;
    if (!splitToFriendlyUnit(bytesValue, friendlyVal, unit))
        return QCoreApplication::translate("misc", "Unknown", "Unknown (size)");
    return Utils::String::fromDouble(friendlyVal, friendlyUnitPrecision(unit))
           + QString::fromUtf8(C_NON_BREAKING_SPACE)
           + unitString(unit, isSpeed);
}

int Utils::Misc::friendlyUnitPrecision(const SizeUnit unit)
{
    // friendlyUnit's number of digits after the decimal point
    switch (unit)
    {
    case SizeUnit::Byte:
        return 0;
    case SizeUnit::KibiByte:
    case SizeUnit::MebiByte:
        return 1;
    case SizeUnit::GibiByte:
        return 2;
    default:
        return 3;
    }
}

qlonglong Utils::Misc::sizeInBytes(qreal size, const Utils::Misc::SizeUnit unit)
{
    for (int i = 0; i < static_cast<int>(unit); ++i)
        size *= 1024;
    return size;
}

bool Utils::Misc::isPreviewable(const QString &extension)
{
    static const QSet<QString> multimediaExtensions =
    {
        "3GP",
        "AAC",
        "AC3",
        "AIF",
        "AIFC",
        "AIFF",
        "ASF",
        "AU",
        "AVI",
        "FLAC",
        "FLV",
        "M3U",
        "M4A",
        "M4P",
        "M4V",
        "MID",
        "MKV",
        "MOV",
        "MP2",
        "MP3",
        "MP4",
        "MPC",
        "MPE",
        "MPEG",
        "MPG",
        "MPP",
        "OGG",
        "OGM",
        "OGV",
        "QT",
        "RA",
        "RAM",
        "RM",
        "RMV",
        "RMVB",
        "SWA",
        "SWF",
        "TS",
        "VOB",
        "WAV",
        "WMA",
        "WMV"
    };
    return multimediaExtensions.contains(extension.toUpper());
}

QString Utils::Misc::userFriendlyDuration(const qlonglong seconds, const qlonglong maxCap)
{
    if (seconds < 0)
        return QString::fromUtf8(C_INFINITY);
    if ((maxCap >= 0) && (seconds >= maxCap))
        return QString::fromUtf8(C_INFINITY);

    if (seconds == 0)
        return "0";

    if (seconds < 60)
        return QCoreApplication::translate("misc", "< 1m", "< 1 minute");

    qlonglong minutes = (seconds / 60);
    if (minutes < 60)
        return QCoreApplication::translate("misc", "%1m", "e.g: 10minutes").arg(QString::number(minutes));

    qlonglong hours = (minutes / 60);
    if (hours < 24)
    {
        minutes -= (hours * 60);
        return QCoreApplication::translate("misc", "%1h %2m", "e.g: 3hours 5minutes").arg(QString::number(hours), QString::number(minutes));
    }

    qlonglong days = (hours / 24);
    if (days < 365)
    {
        hours -= (days * 24);
        return QCoreApplication::translate("misc", "%1d %2h", "e.g: 2days 10hours").arg(QString::number(days), QString::number(hours));
    }

    qlonglong years = (days / 365);
    days -= (years * 365);
    return QCoreApplication::translate("misc", "%1y %2d", "e.g: 2years 10days").arg(QString::number(years), QString::number(days));
}

QString Utils::Misc::getUserIDString()
{
    QString uid = "0";
#ifdef Q_OS_WIN
    const int UNLEN = 256;
    WCHAR buffer[UNLEN + 1] = {0};
    DWORD buffer_len = sizeof(buffer) / sizeof(*buffer);
    if (GetUserNameW(buffer, &buffer_len))
        uid = QString::fromWCharArray(buffer);
#else
    uid = QString::number(getuid());
#endif
    return uid;
}

QString Utils::Misc::parseHtmlLinks(const QString &rawText)
{
    QString result = rawText;
    static const QRegularExpression reURL(
        "(\\s|^)"                                             // start with whitespace or beginning of line
        "("
        "("                                              // case 1 -- URL with scheme
        "(http(s?))\\://"                            // start with scheme
        "([a-zA-Z0-9_-]+\\.)+"                       //  domainpart.  at least one of these must exist
        "([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"              //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
        ")"
        "|"
        "("                                             // case 2a -- no scheme, contains common TLD  example.com
        "([a-zA-Z0-9_-]+\\.)+"                      //  domainpart.  at least one of these must exist
        "(?="                                       //  must be followed by TLD
        "AERO|aero|"                          // N.B. assertions are non-capturing
        "ARPA|arpa|"
        "ASIA|asia|"
        "BIZ|biz|"
        "CAT|cat|"
        "COM|com|"
        "COOP|coop|"
        "EDU|edu|"
        "GOV|gov|"
        "INFO|info|"
        "INT|int|"
        "JOBS|jobs|"
        "MIL|mil|"
        "MOBI|mobi|"
        "MUSEUM|museum|"
        "NAME|name|"
        "NET|net|"
        "ORG|org|"
        "PRO|pro|"
        "RO|ro|"
        "RU|ru|"
        "TEL|tel|"
        "TRAVEL|travel"
        ")"
        "([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"             //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
        ")"
        "|"
        "("                                             // case 2b no scheme, no TLD, must have at least 2 alphanum strings plus uncommon TLD string  --> del.icio.us
        "([a-zA-Z0-9_-]+\\.) {2,}"                   // 2 or more domainpart.   --> del.icio.
        "[a-zA-Z]{2,}"                              // one ab  (2 char or longer) --> us
        "([a-zA-Z0-9\\?%=&/_\\.:#;-]*)"             // everything to 1st non-URI char, maybe nothing  in case of del.icio.us/path
        ")"
        ")"
        );

    // Capture links
    result.replace(reURL, "\\1<a href=\"\\2\">\\2</a>");

    // Capture links without scheme
    static const QRegularExpression reNoScheme("<a\\s+href=\"(?!https?)([a-zA-Z0-9\\?%=&/_\\.-:#]+)\\s*\">");
    result.replace(reNoScheme, "<a href=\"http://\\1\">");

    // to preserve plain text formatting
    result = "<p style=\"white-space: pre-wrap;\">" + result + "</p>";
    return result;
}

QString Utils::Misc::osName()
{
    // static initialization for usage in signal handler
    static const QString name =
        QString("%1 %2 %3")
        .arg(QSysInfo::prettyProductName()
            , QSysInfo::kernelVersion()
            , QSysInfo::currentCpuArchitecture());
    return name;
}

QString Utils::Misc::boostVersionString()
{
    // static initialization for usage in signal handler
    static const QString ver = QString("%1.%2.%3")
        .arg(QString::number(BOOST_VERSION / 100000)
            , QString::number((BOOST_VERSION / 100) % 1000)
            , QString::number(BOOST_VERSION % 100));
    return ver;
}

QString Utils::Misc::libtorrentVersionString()
{
    // static initialization for usage in signal handler
    static const auto version {QString::fromLatin1(lt::version())};
    return version;
}

QString Utils::Misc::opensslVersionString()
{
#if (OPENSSL_VERSION_NUMBER >= 0x1010000f)
    static const auto version {QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION))};
#else
    static const auto version {QString::fromLatin1(SSLeay_version(SSLEAY_VERSION))};
#endif
    return version.splitRef(' ', QString::SkipEmptyParts)[1].toString();
}

QString Utils::Misc::zlibVersionString()
{
    // static initialization for usage in signal handler
    static const auto version {QString::fromLatin1(zlibVersion())};
    return version;
}

#ifdef Q_OS_WIN
QString Utils::Misc::windowsSystemPath()
{
    static const QString path = []() -> QString
    {
        WCHAR systemPath[MAX_PATH] = {0};
        GetSystemDirectoryW(systemPath, sizeof(systemPath) / sizeof(WCHAR));
        return QString::fromWCharArray(systemPath);
    }();
    return path;
}
#endif
