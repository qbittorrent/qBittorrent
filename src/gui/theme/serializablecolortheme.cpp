/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "serializablecolortheme.h"

#include <QLoggingCategory>
#include <QMetaEnum>
#include <QSettings>

#include <libtorrent/version.hpp>

#include "base/bittorrent/torrenthandle.h"
#include "base/logger.h"

#include "colorprovider_p.h"
#include "serializabletheme.h"
#include "themeprovider.h"

namespace
{
    const QByteArray torrentStateColorsGroup("TorrentState/");
    const QByteArray msgTypeColorsGroup("LogMessageType/");
    const QByteArray downloadProggressBarColorsGroup("DownloadProgressBar/");

    const QLatin1String defaultLightColorThemeName ("Default (Light)");
    const QLatin1String defaultDarkColorThemeName ("Default (Dark)");

    Theme::ColorThemeElement toThemeElement(Theme::DownloadProgressBarElement element)
    {
        switch (element) {
        case Theme::DownloadProgressBarElement::Background:
            return Theme::ColorThemeElement::DownloadProgressBarElementBackground;
        case Theme::DownloadProgressBarElement::Border:
            return Theme::ColorThemeElement::DownloadProgressBarElementBorder;
        case Theme::DownloadProgressBarElement::Complete:
            return Theme::ColorThemeElement::DownloadProgressBarElementComplete;
        case Theme::DownloadProgressBarElement::Incomplete:
            return Theme::ColorThemeElement::DownloadProgressBarElementIncomplete;
        default:
            throw std::logic_error("Unexpected DownloadProgressBarElement value");
        }
    }

    Theme::ColorThemeElement toThemeElement(Log::MsgType messageType)
    {
        switch (messageType) {
        case Log::MsgType::ALL:
            return Theme::ColorThemeElement::MsgTypeALL;
        case Log::MsgType::NORMAL:
            return Theme::ColorThemeElement::MsgTypeNORMAL;
        case Log::MsgType::INFO:
            return Theme::ColorThemeElement::MsgTypeINFO;
        case Log::MsgType::WARNING:
            return Theme::ColorThemeElement::MsgTypeWARNING;
        case Log::MsgType::CRITICAL:
            return Theme::ColorThemeElement::MsgTypeCRITICAL;
        default:
            throw std::logic_error("Unexpected Log::MsgType value");
        }
    }

    Theme::ColorThemeElement toThemeElement(BitTorrent::TorrentState state)
    {
        switch (state) {
        case BitTorrent::TorrentState::Unknown:
            return Theme::ColorThemeElement::TorrentStateUnknown;
        case BitTorrent::TorrentState::Error:
            return Theme::ColorThemeElement::TorrentStateError;
        case BitTorrent::TorrentState::MissingFiles:
            return Theme::ColorThemeElement::TorrentStateMissingFiles;
        case BitTorrent::TorrentState::Uploading:
            return Theme::ColorThemeElement::TorrentStateUploading;
        case BitTorrent::TorrentState::PausedUploading:
            return Theme::ColorThemeElement::TorrentStatePausedUploading;
        case BitTorrent::TorrentState::QueuedUploading:
            return Theme::ColorThemeElement::TorrentStateQueuedUploading;
        case BitTorrent::TorrentState::StalledUploading:
            return Theme::ColorThemeElement::TorrentStateStalledUploading;
        case BitTorrent::TorrentState::CheckingUploading:
            return Theme::ColorThemeElement::TorrentStateCheckingUploading;
        case BitTorrent::TorrentState::ForcedUploading:
            return Theme::ColorThemeElement::TorrentStateForcedUploading;
        case BitTorrent::TorrentState::Allocating:
            return Theme::ColorThemeElement::TorrentStateAllocating;
        case BitTorrent::TorrentState::Downloading:
            return Theme::ColorThemeElement::TorrentStateDownloading;
        case BitTorrent::TorrentState::DownloadingMetadata:
            return Theme::ColorThemeElement::TorrentStateDownloadingMetadata;
        case BitTorrent::TorrentState::PausedDownloading:
            return Theme::ColorThemeElement::TorrentStatePausedDownloading;
        case BitTorrent::TorrentState::QueuedDownloading:
            return Theme::ColorThemeElement::TorrentStateQueuedDownloading;
        case BitTorrent::TorrentState::StalledDownloading:
            return Theme::ColorThemeElement::TorrentStateStalledDownloading;
        case BitTorrent::TorrentState::CheckingDownloading:
            return Theme::ColorThemeElement::TorrentStateCheckingDownloading;
        case BitTorrent::TorrentState::ForcedDownloading:
            return Theme::ColorThemeElement::TorrentStateForcedDownloading;
    #if LIBTORRENT_VERSION_NUM < 10100
        case BitTorrent::TorrentState::QueuedForChecking:
            return Theme::ColorThemeElement::TorrentStateQueuedForChecking;
    #endif
        case BitTorrent::TorrentState::CheckingResumeData:
            return Theme::ColorThemeElement::TorrentStateCheckingResumeData;
        default:
            throw std::logic_error("Unexpected TorrentState value");
        }
    }
}

Theme::SerializableColorTheme::SerializableColorTheme(const QString &name)
    : BaseSerializableTheme(name, elementNames())
{
}

void Theme::SerializableColorTheme::applicationPaletteChanged()
{
    updateCache();
}

Theme::SerializableColorTheme::BaseSerializableTheme::NamesMap
Theme::SerializableColorTheme::elementNames()
{
    const auto makeTorrentStateNamePair = [](BitTorrent::TorrentState st)
    {
        return std::make_pair(QByteArray(torrentStateColorsGroup + colorSaveName(st)), static_cast<int>(toThemeElement(st)));
    };

    const auto makeMsgTypeNamePair = [](Log::MsgType mt)
    {
        return std::make_pair(QByteArray(msgTypeColorsGroup + colorSaveName(mt)), static_cast<int>(toThemeElement(mt)));
    };

    const auto makeDownloadProgressBarNamePair = [](DownloadProgressBarElement e)
    {
        return std::make_pair(QByteArray(downloadProggressBarColorsGroup + colorSaveName(e)), static_cast<int>(toThemeElement(e)));
    };

    return {
        makeTorrentStateNamePair(BitTorrent::TorrentState::Unknown),
        makeTorrentStateNamePair(BitTorrent::TorrentState::Error),
        makeTorrentStateNamePair(BitTorrent::TorrentState::MissingFiles),
        makeTorrentStateNamePair(BitTorrent::TorrentState::Uploading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::PausedUploading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::QueuedUploading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::StalledUploading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::CheckingUploading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::ForcedUploading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::Allocating),
        makeTorrentStateNamePair(BitTorrent::TorrentState::Downloading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::DownloadingMetadata),
        makeTorrentStateNamePair(BitTorrent::TorrentState::PausedDownloading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::QueuedDownloading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::StalledDownloading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::CheckingDownloading),
        makeTorrentStateNamePair(BitTorrent::TorrentState::ForcedDownloading),
#if LIBTORRENT_VERSION_NUM < 10100
        makeTorrentStateNamePair(BitTorrent::TorrentState::QueuedForChecking),
#endif
        makeTorrentStateNamePair(BitTorrent::TorrentState::CheckingResumeData),

        makeMsgTypeNamePair(Log::MsgType::ALL),
        makeMsgTypeNamePair(Log::MsgType::NORMAL),
        makeMsgTypeNamePair(Log::MsgType::INFO),
        makeMsgTypeNamePair(Log::MsgType::WARNING),
        makeMsgTypeNamePair(Log::MsgType::CRITICAL),

        makeDownloadProgressBarNamePair(DownloadProgressBarElement::Background),
        makeDownloadProgressBarNamePair(DownloadProgressBarElement::Border),
        makeDownloadProgressBarNamePair(DownloadProgressBarElement::Complete),
        makeDownloadProgressBarNamePair(DownloadProgressBarElement::Incomplete)
    };
}

Theme::ThemeInfo Theme::SerializableColorTheme::info() const
{
    return BaseSerializableTheme::info();
}

QByteArray Theme::SerializableColorTheme::colorSaveName(BitTorrent::TorrentState state)
{
    // TODO Qt 5.8+: use Q_NAMESPACE + Q_ENUM_NS
    //  static QMetaEnum meta = QMetaEnum::fromType<BitTorrent::TorrentState>();
    //  return QString::fromLatin1(meta.key(static_cast<int>(state)));
    switch (state) {
    case BitTorrent::TorrentState::Unknown:
        return "Unknown";
    case BitTorrent::TorrentState::Error:
        return "Error";
    case BitTorrent::TorrentState::MissingFiles:
        return "MissingFiles";
    case BitTorrent::TorrentState::Uploading:
        return "Uploading";
    case BitTorrent::TorrentState::PausedUploading:
        return "PausedUploading";
    case BitTorrent::TorrentState::QueuedUploading:
        return "QueuedUploading";
    case BitTorrent::TorrentState::StalledUploading:
        return "StalledUploading";
    case BitTorrent::TorrentState::CheckingUploading:
        return "CheckingUploading";
    case BitTorrent::TorrentState::ForcedUploading:
        return "ForcedUploading";
    case BitTorrent::TorrentState::Allocating:
        return "Allocating";
    case BitTorrent::TorrentState::Downloading:
        return "Downloading";
    case BitTorrent::TorrentState::DownloadingMetadata:
        return "DownloadingMetadata";
    case BitTorrent::TorrentState::PausedDownloading:
        return "PausedDownloading";
    case BitTorrent::TorrentState::QueuedDownloading:
        return "QueuedDownloading";
    case BitTorrent::TorrentState::StalledDownloading:
        return "StalledDownloading";
    case BitTorrent::TorrentState::CheckingDownloading:
        return "CheckingDownloading";
    case BitTorrent::TorrentState::ForcedDownloading:
        return "ForcedDownloading";
#if LIBTORRENT_VERSION_NUM < 10100
    case BitTorrent::TorrentState::QueuedForChecking:
        return "QueuedForChecking";
#endif
    case BitTorrent::TorrentState::CheckingResumeData:
        return "CheckingResumeData";
    default:
        throw std::logic_error("Unexpected TorrentState value");
    }
}

QByteArray Theme::SerializableColorTheme::colorSaveName(Log::MsgType msgType)
{
    // TODO Qt 5.8+: use Q_NAMESPACE + Q_ENUM_NS
    //  static QMetaEnum meta = QMetaEnum::fromType<Log::MsgType>();
    //  return QString::fromLatin1(meta.key(static_cast<int>(msgType)));
    switch (msgType) {
    case Log::MsgType::ALL:
        return "ALL";
    case Log::MsgType::NORMAL:
        return "NORMAL";
    case Log::MsgType::INFO:
        return "INFO";
    case Log::MsgType::WARNING:
        return "WARNING";
    case Log::MsgType::CRITICAL:
        return "CRITICAL";
    default:
        throw std::logic_error("Unexpected Log::MsgType value");
    }
}

QByteArray Theme::SerializableColorTheme::colorSaveName(DownloadProgressBarElement element)
{
    // TODO Qt 5.8+: use Q_NAMESPACE + Q_ENUM_NS
    //  static QMetaEnum meta = QMetaEnum::fromType<DownloadProgressBarElement>();
    //  return QString::fromLatin1(meta.key(static_cast<int>(element)));
    switch (element) {
    case DownloadProgressBarElement::Background:
        return "Background";
    case DownloadProgressBarElement::Border:
        return "Border";
    case DownloadProgressBarElement::Complete:
        return "Complete";
    case DownloadProgressBarElement::Incomplete:
        return "Incomplete";
    default:
        throw std::logic_error("Unexpected DownloadProgressBarElement value");
    }
}

QColor Theme::SerializableColorTheme::downloadProgressBarColor(const DownloadProgressBarElement elem) const
{
    return element(toThemeElement(elem));
}

QColor Theme::SerializableColorTheme::logMessageColor(const Log::MsgType messageType) const
{
    return element(toThemeElement(messageType));
}

QColor Theme::SerializableColorTheme::torrentStateColor(const BitTorrent::TorrentState state) const
{
    return element(toThemeElement(state));
}

