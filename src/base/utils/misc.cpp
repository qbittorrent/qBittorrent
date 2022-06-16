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

#include <optional>

#ifdef Q_OS_WIN
#include <memory>

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
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QSysInfo>
#include <QVector>

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
#include <QDBusInterface>
#endif

#include "base/types.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
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
    struct SplitToFriendlyUnitResult
    {
        qreal value;
        Utils::Misc::SizeUnit unit;
    };

    std::optional<SplitToFriendlyUnitResult> splitToFriendlyUnit(const qint64 bytes)
    {
        if (bytes < 0)
            return std::nullopt;

        int i = 0;
        auto value = static_cast<qreal>(bytes);

        while ((value >= 1024) && (i < static_cast<int>(Utils::Misc::SizeUnit::ExbiByte)))
        {
            value /= 1024;
            ++i;
        }
        return {{value, static_cast<Utils::Misc::SizeUnit>(i)}};
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
        ::SetSuspendState(FALSE, FALSE, FALSE);
    }
    else if (action == ShutdownDialogAction::Hibernate)
    {
        ::SetSuspendState(TRUE, FALSE, FALSE);
    }
    else
    {
        const QString msg = QCoreApplication::translate("misc", "qBittorrent will shutdown the computer now because all downloads are complete.");
        auto msgWchar = std::make_unique<wchar_t[]>(msg.length() + 1);
        msg.toWCharArray(msgWchar.get());
        ::InitiateSystemShutdownW(nullptr, msgWchar.get(), 10, TRUE, FALSE);
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
        QDBusInterface login1Iface(u"org.freedesktop.login1"_qs, u"/org/freedesktop/login1"_qs,
                                   u"org.freedesktop.login1.Manager"_qs, QDBusConnection::systemBus());
        if (login1Iface.isValid())
        {
            if (action == ShutdownDialogAction::Suspend)
                login1Iface.call(u"Suspend"_qs, false);
            else
                login1Iface.call(u"Hibernate"_qs, false);
            return;
        }
        // Else, other recent systems use UPower
        QDBusInterface upowerIface(u"org.freedesktop.UPower"_qs, u"/org/freedesktop/UPower"_qs,
                                   u"org.freedesktop.UPower"_qs, QDBusConnection::systemBus());
        if (upowerIface.isValid())
        {
            if (action == ShutdownDialogAction::Suspend)
                upowerIface.call(u"Suspend"_qs);
            else
                upowerIface.call(u"Hibernate"_qs);
            return;
        }
        // HAL (older systems)
        QDBusInterface halIface(u"org.freedesktop.Hal"_qs, u"/org/freedesktop/Hal/devices/computer"_qs,
                                u"org.freedesktop.Hal.Device.SystemPowerManagement"_qs,
                                QDBusConnection::systemBus());
        if (action == ShutdownDialogAction::Suspend)
            halIface.call(u"Suspend"_qs, 5);
        else
            halIface.call(u"Hibernate"_qs);
    }
    else
    {
        // Some recent systems use systemd's logind
        QDBusInterface login1Iface(u"org.freedesktop.login1"_qs, u"/org/freedesktop/login1"_qs,
                                   u"org.freedesktop.login1.Manager"_qs, QDBusConnection::systemBus());
        if (login1Iface.isValid())
        {
            login1Iface.call(u"PowerOff"_qs, false);
            return;
        }
        // Else, other recent systems use ConsoleKit
        QDBusInterface consolekitIface(u"org.freedesktop.ConsoleKit"_qs, u"/org/freedesktop/ConsoleKit/Manager"_qs,
                                       u"org.freedesktop.ConsoleKit.Manager"_qs, QDBusConnection::systemBus());
        if (consolekitIface.isValid())
        {
            consolekitIface.call(u"Stop"_qs);
            return;
        }
        // HAL (older systems)
        QDBusInterface halIface(u"org.freedesktop.Hal"_qs, u"/org/freedesktop/Hal/devices/computer"_qs,
                                u"org.freedesktop.Hal.Device.SystemPowerManagement"_qs,
                                QDBusConnection::systemBus());
        halIface.call(u"Shutdown"_qs);
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

QString Utils::Misc::friendlyUnit(const qint64 bytes, const bool isSpeed)
{
    const std::optional<SplitToFriendlyUnitResult> result = splitToFriendlyUnit(bytes);
    if (!result)
        return QCoreApplication::translate("misc", "Unknown", "Unknown (size)");
    return Utils::String::fromDouble(result->value, friendlyUnitPrecision(result->unit))
           + C_NON_BREAKING_SPACE
           + unitString(result->unit, isSpeed);
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

bool Utils::Misc::isPreviewable(const Path &filePath)
{
    const QString mime = QMimeDatabase().mimeTypeForFile(filePath.data(), QMimeDatabase::MatchExtension).name();

    if (mime.startsWith(u"audio", Qt::CaseInsensitive)
        || mime.startsWith(u"video", Qt::CaseInsensitive))
    {
        return true;
    }

    const QSet<QString> multimediaExtensions =
    {
        u".3GP"_qs,
        u".AAC"_qs,
        u".AC3"_qs,
        u".AIF"_qs,
        u".AIFC"_qs,
        u".AIFF"_qs,
        u".ASF"_qs,
        u".AU"_qs,
        u".AVI"_qs,
        u".FLAC"_qs,
        u".FLV"_qs,
        u".M3U"_qs,
        u".M4A"_qs,
        u".M4P"_qs,
        u".M4V"_qs,
        u".MID"_qs,
        u".MKV"_qs,
        u".MOV"_qs,
        u".MP2"_qs,
        u".MP3"_qs,
        u".MP4"_qs,
        u".MPC"_qs,
        u".MPE"_qs,
        u".MPEG"_qs,
        u".MPG"_qs,
        u".MPP"_qs,
        u".OGG"_qs,
        u".OGM"_qs,
        u".OGV"_qs,
        u".QT"_qs,
        u".RA"_qs,
        u".RAM"_qs,
        u".RM"_qs,
        u".RMV"_qs,
        u".RMVB"_qs,
        u".SWA"_qs,
        u".SWF"_qs,
        u".TS"_qs,
        u".VOB"_qs,
        u".WAV"_qs,
        u".WMA"_qs,
        u".WMV"_qs
    };
    return multimediaExtensions.contains(filePath.extension().toUpper());
}

QString Utils::Misc::userFriendlyDuration(const qlonglong seconds, const qlonglong maxCap)
{
    if (seconds < 0)
        return C_INFINITY;
    if ((maxCap >= 0) && (seconds >= maxCap))
        return C_INFINITY;

    if (seconds == 0)
        return u"0"_qs;

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
    QString uid = u"0"_qs;
#ifdef Q_OS_WIN
    const int UNLEN = 256;
    WCHAR buffer[UNLEN + 1] = {0};
    DWORD buffer_len = sizeof(buffer) / sizeof(*buffer);
    if (::GetUserNameW(buffer, &buffer_len))
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
        u"(\\s|^)"                                             // start with whitespace or beginning of line
        u"("
        u"("                                              // case 1 -- URL with scheme
        u"(http(s?))\\://"                            // start with scheme
        u"([a-zA-Z0-9_-]+\\.)+"                       //  domainpart.  at least one of these must exist
        u"([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"              //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
        u")"
        u"|"
        u"("                                             // case 2a -- no scheme, contains common TLD  example.com
        u"([a-zA-Z0-9_-]+\\.)+"                      //  domainpart.  at least one of these must exist
        u"(?="                                       //  must be followed by TLD
        u"AERO|aero|"                          // N.B. assertions are non-capturing
        u"ARPA|arpa|"
        u"ASIA|asia|"
        u"BIZ|biz|"
        u"CAT|cat|"
        u"COM|com|"
        u"COOP|coop|"
        u"EDU|edu|"
        u"GOV|gov|"
        u"INFO|info|"
        u"INT|int|"
        u"JOBS|jobs|"
        u"MIL|mil|"
        u"MOBI|mobi|"
        u"MUSEUM|museum|"
        u"NAME|name|"
        u"NET|net|"
        u"ORG|org|"
        u"PRO|pro|"
        u"RO|ro|"
        u"RU|ru|"
        u"TEL|tel|"
        u"TRAVEL|travel"
        u")"
        u"([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"             //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
        u")"
        u"|"
        u"("                                             // case 2b no scheme, no TLD, must have at least 2 alphanum strings plus uncommon TLD string  --> del.icio.us
        u"([a-zA-Z0-9_-]+\\.) {2,}"                   // 2 or more domainpart.   --> del.icio.
        u"[a-zA-Z]{2,}"                              // one ab  (2 char or longer) --> us
        u"([a-zA-Z0-9\\?%=&/_\\.:#;-]*)"             // everything to 1st non-URI char, maybe nothing  in case of del.icio.us/path
        u")"
        u")"_qs
        );

    // Capture links
    result.replace(reURL, u"\\1<a href=\"\\2\">\\2</a>"_qs);

    // Capture links without scheme
    const QRegularExpression reNoScheme(u"<a\\s+href=\"(?!https?)([a-zA-Z0-9\\?%=&/_\\.-:#]+)\\s*\">"_qs);
    result.replace(reNoScheme, u"<a href=\"http://\\1\">"_qs);

    // to preserve plain text formatting
    result = u"<p style=\"white-space: pre-wrap;\">" + result + u"</p>";
    return result;
}

QString Utils::Misc::osName()
{
    // static initialization for usage in signal handler
    static const QString name =
        u"%1 %2 %3"_qs
        .arg(QSysInfo::prettyProductName()
            , QSysInfo::kernelVersion()
            , QSysInfo::currentCpuArchitecture());
    return name;
}

QString Utils::Misc::boostVersionString()
{
    // static initialization for usage in signal handler
    static const QString ver = u"%1.%2.%3"_qs
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
    return version.section(u' ', 1, 1);
}

QString Utils::Misc::zlibVersionString()
{
    // static initialization for usage in signal handler
    static const auto version {QString::fromLatin1(zlibVersion())};
    return version;
}

#ifdef Q_OS_WIN
Path Utils::Misc::windowsSystemPath()
{
    static const Path path = []() -> Path
    {
        WCHAR systemPath[MAX_PATH] = {0};
        GetSystemDirectoryW(systemPath, sizeof(systemPath) / sizeof(WCHAR));
        return Path(QString::fromWCharArray(systemPath));
    }();
    return path;
}
#endif
