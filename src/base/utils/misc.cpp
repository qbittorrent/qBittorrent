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

#include <boost/version.hpp>
#include <libtorrent/version.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <zlib.h>

#include <QtAssert>
#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringView>
#include <QSysInfo>

#include "base/net/downloadmanager.h"
#include "base/path.h"
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
    struct SplitToFriendlyUnitResult
    {
        qreal value;
        Utils::Misc::SizeUnit unit;
    };

    std::optional<SplitToFriendlyUnitResult> splitToFriendlyUnit(const qint64 bytes, const int unitThreshold = 1024)
    {
        if (bytes < 0)
            return std::nullopt;

        int i = 0;
        auto value = static_cast<qreal>(bytes);

        while ((value >= unitThreshold) && (i < static_cast<int>(Utils::Misc::SizeUnit::ExbiByte)))
        {
            value /= 1024;
            ++i;
        }
        return {{value, static_cast<Utils::Misc::SizeUnit>(i)}};
    }
}

QString Utils::Misc::unitString(const SizeUnit unit, const bool isSpeed)
{
    const auto &unitString = units[static_cast<int>(unit)];
    QString ret = QCoreApplication::translate("misc", unitString.source, unitString.comment);
    if (isSpeed)
        ret += QCoreApplication::translate("misc", "/s", "per second");
    return ret;
}

QString Utils::Misc::friendlyUnit(const qint64 bytes, const bool isSpeed, const int precision)
{
    const std::optional<SplitToFriendlyUnitResult> result = splitToFriendlyUnit(bytes);
    if (!result)
        return QCoreApplication::translate("misc", "Unknown", "Unknown (size)");

    const int digitPrecision = (precision >= 0) ? precision : friendlyUnitPrecision(result->unit);
    return Utils::String::fromDouble(result->value, digitPrecision)
           + QChar::Nbsp + unitString(result->unit, isSpeed);
}

QString Utils::Misc::friendlyUnitCompact(const qint64 bytes)
{
    // avoid 1000-1023 values, use next larger unit instead
    const std::optional<SplitToFriendlyUnitResult> result = splitToFriendlyUnit(bytes, 1000);
    if (!result)
        return QCoreApplication::translate("misc", "Unknown", "Unknown (size)");

    int precision = 0;          // >= 100
    if (result->value < 10)
        precision = 2;          // 0 - 9.99
    if (result->value < 100)
        precision = 1;          // 10 - 99.9

    return Utils::String::fromDouble(result->value, precision)
           // use only one character for unit representation
           + QChar::Nbsp + unitString(result->unit, false)[0];
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
        u".3GP"_s,
        u".AAC"_s,
        u".AC3"_s,
        u".AIF"_s,
        u".AIFC"_s,
        u".AIFF"_s,
        u".ASF"_s,
        u".AU"_s,
        u".AVI"_s,
        u".FLAC"_s,
        u".FLV"_s,
        u".M3U"_s,
        u".M4A"_s,
        u".M4P"_s,
        u".M4V"_s,
        u".MID"_s,
        u".MKV"_s,
        u".MOV"_s,
        u".MP2"_s,
        u".MP3"_s,
        u".MP4"_s,
        u".MPC"_s,
        u".MPE"_s,
        u".MPEG"_s,
        u".MPG"_s,
        u".MPP"_s,
        u".OGG"_s,
        u".OGM"_s,
        u".OGV"_s,
        u".QT"_s,
        u".RA"_s,
        u".RAM"_s,
        u".RM"_s,
        u".RMV"_s,
        u".RMVB"_s,
        u".SWA"_s,
        u".SWF"_s,
        u".TS"_s,
        u".VOB"_s,
        u".WAV"_s,
        u".WMA"_s,
        u".WMV"_s
    };
    return multimediaExtensions.contains(filePath.extension().toUpper());
}

bool Utils::Misc::isTorrentLink(const QString &str)
{
    return str.startsWith(u"magnet:", Qt::CaseInsensitive)
        || str.endsWith(TORRENT_FILE_EXTENSION, Qt::CaseInsensitive)
        || (!str.startsWith(u"file:", Qt::CaseInsensitive)
            && Net::DownloadManager::hasSupportedScheme(str));
}

QString Utils::Misc::userFriendlyDuration(const qlonglong seconds, const qlonglong maxCap, const TimeResolution resolution)
{
    if (seconds < 0)
        return C_INFINITY;
    if ((maxCap >= 0) && (seconds >= maxCap))
        return C_INFINITY;

    if (seconds == 0)
        return u"0"_s;

    if (seconds < 60)
    {
        if (resolution == TimeResolution::Minutes)
            return QCoreApplication::translate("misc", "< 1m", "< 1 minute");

        return QCoreApplication::translate("misc", "%1s", "e.g: 10 seconds").arg(QString::number(seconds));
    }

    qlonglong minutes = (seconds / 60);
    if (minutes < 60)
        return QCoreApplication::translate("misc", "%1m", "e.g: 10 minutes").arg(QString::number(minutes));

    qlonglong hours = (minutes / 60);
    if (hours < 24)
    {
        minutes -= (hours * 60);
        return QCoreApplication::translate("misc", "%1h %2m", "e.g: 3 hours 5 minutes").arg(QString::number(hours), QString::number(minutes));
    }

    qlonglong days = (hours / 24);
    if (days < 365)
    {
        hours -= (days * 24);
        return QCoreApplication::translate("misc", "%1d %2h", "e.g: 2 days 10 hours").arg(QString::number(days), QString::number(hours));
    }

    qlonglong years = (days / 365);
    days -= (years * 365);
    return QCoreApplication::translate("misc", "%1y %2d", "e.g: 2 years 10 days").arg(QString::number(years), QString::number(days));
}

QString Utils::Misc::languageToLocalizedString(const QStringView localeStr)
{
    if (localeStr.startsWith(u"eo", Qt::CaseInsensitive))
    {
        // QLocale doesn't work with that locale. Esperanto isn't a "real" language.
        return C_LOCALE_ESPERANTO;
    }

    if (localeStr.startsWith(u"ltg", Qt::CaseInsensitive))
    {
        // QLocale doesn't work with that locale.
        return C_LOCALE_LATGALIAN;
    }

    const QLocale locale {localeStr};
    switch (locale.language())
    {
    case QLocale::Arabic: return C_LOCALE_ARABIC;
    case QLocale::Armenian: return C_LOCALE_ARMENIAN;
    case QLocale::Azerbaijani: return C_LOCALE_AZERBAIJANI;
    case QLocale::Basque: return C_LOCALE_BASQUE;
    case QLocale::Bulgarian: return C_LOCALE_BULGARIAN;
    case QLocale::Byelorussian: return C_LOCALE_BYELORUSSIAN;
    case QLocale::Catalan: return C_LOCALE_CATALAN;
    case QLocale::Chinese:
        switch (locale.territory())
        {
        case QLocale::China: return C_LOCALE_CHINESE_SIMPLIFIED;
        case QLocale::HongKong: return C_LOCALE_CHINESE_TRADITIONAL_HK;
        default: return C_LOCALE_CHINESE_TRADITIONAL_TW;
        }
    case QLocale::Croatian: return C_LOCALE_CROATIAN;
    case QLocale::Czech: return C_LOCALE_CZECH;
    case QLocale::Danish: return C_LOCALE_DANISH;
    case QLocale::Dutch: return C_LOCALE_DUTCH;
    case QLocale::English:
        switch (locale.territory())
        {
        case QLocale::Australia: return C_LOCALE_ENGLISH_AUSTRALIA;
        case QLocale::UnitedKingdom: return C_LOCALE_ENGLISH_UNITEDKINGDOM;
        default: return C_LOCALE_ENGLISH;
        }
    case QLocale::Estonian: return C_LOCALE_ESTONIAN;
    case QLocale::Finnish: return C_LOCALE_FINNISH;
    case QLocale::French: return C_LOCALE_FRENCH;
    case QLocale::Galician: return C_LOCALE_GALICIAN;
    case QLocale::Georgian: return C_LOCALE_GEORGIAN;
    case QLocale::German: return C_LOCALE_GERMAN;
    case QLocale::Greek: return C_LOCALE_GREEK;
    case QLocale::Hebrew: return C_LOCALE_HEBREW;
    case QLocale::Hindi: return C_LOCALE_HINDI;
    case QLocale::Hungarian: return C_LOCALE_HUNGARIAN;
    case QLocale::Icelandic: return C_LOCALE_ICELANDIC;
    case QLocale::Indonesian: return C_LOCALE_INDONESIAN;
    case QLocale::Italian: return C_LOCALE_ITALIAN;
    case QLocale::Japanese: return C_LOCALE_JAPANESE;
    case QLocale::Korean: return C_LOCALE_KOREAN;
    case QLocale::Latvian: return C_LOCALE_LATVIAN;
    case QLocale::Lithuanian: return C_LOCALE_LITHUANIAN;
    case QLocale::Malay: return C_LOCALE_MALAY;
    case QLocale::Mongolian: return C_LOCALE_MONGOLIAN;
    case QLocale::NorwegianBokmal: return C_LOCALE_NORWEGIAN;
    case QLocale::Occitan: return C_LOCALE_OCCITAN;
    case QLocale::Persian: return C_LOCALE_PERSIAN;
    case QLocale::Polish: return C_LOCALE_POLISH;
    case QLocale::Portuguese:
        if (locale.territory() == QLocale::Brazil)
            return C_LOCALE_PORTUGUESE_BRAZIL;
        return C_LOCALE_PORTUGUESE;
    case QLocale::Romanian: return C_LOCALE_ROMANIAN;
    case QLocale::Russian: return C_LOCALE_RUSSIAN;
    case QLocale::Serbian: return C_LOCALE_SERBIAN;
    case QLocale::Slovak: return C_LOCALE_SLOVAK;
    case QLocale::Slovenian: return C_LOCALE_SLOVENIAN;
    case QLocale::Spanish: return C_LOCALE_SPANISH;
    case QLocale::Swedish: return C_LOCALE_SWEDISH;
    case QLocale::Thai: return C_LOCALE_THAI;
    case QLocale::Turkish: return C_LOCALE_TURKISH;
    case QLocale::Ukrainian: return C_LOCALE_UKRAINIAN;
    case QLocale::Uzbek: return C_LOCALE_UZBEK;
    case QLocale::Vietnamese: return C_LOCALE_VIETNAMESE;
    default:
        const QString lang = QLocale::languageToString(locale.language());
        qWarning() << "Unrecognized language name: " << lang;
        return lang;
    }
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
        u")"_s
        );

    // Capture links
    result.replace(reURL, u"\\1<a href=\"\\2\">\\2</a>"_s);

    // Capture links without scheme
    const QRegularExpression reNoScheme(u"<a\\s+href=\"(?!https?)([a-zA-Z0-9\\?%=&/_\\.-:#]+)\\s*\">"_s);
    result.replace(reNoScheme, u"<a href=\"http://\\1\">"_s);

    // to preserve plain text formatting
    result = u"<p style=\"white-space: pre-wrap;\">" + result + u"</p>";
    return result;
}

QString Utils::Misc::osName()
{
    // static initialization for usage in signal handler
    static const QString name =
        u"%1 %2 %3"_s
        .arg(QSysInfo::prettyProductName()
            , QSysInfo::kernelVersion()
            , QSysInfo::currentCpuArchitecture());
    return name;
}

QString Utils::Misc::boostVersionString()
{
    // static initialization for usage in signal handler
    static const QString ver = u"%1.%2.%3"_s
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
    // static initialization for usage in signal handler
    static const auto version {QString::fromLatin1(::OpenSSL_version(OPENSSL_VERSION))
        .section(u' ', 1, 1)};
    return version;
}

QString Utils::Misc::zlibVersionString()
{
    // static initialization for usage in signal handler
    static const auto version {QString::fromLatin1(zlibVersion())};
    return version;
}
